//
// Created by 赵程 on 2021/4/21.
//

#include <atomic>
#include <vector>
#include <iostream>
#include <thread>
#include <parallel/algorithm>

#include <rpc/server.h>
#include <rpc/client.h>
#include <args-parser/Args/all.hpp>

#include "LRUCache.hpp"
#include "Common.h"

template<typename Key=KeyT, typename Value=ValueT>
class DHIServer {
//    using BloomFilter = cuckoofilter::CuckooFilter<uint64_t, 8, cuckoofilter::SingleTable, SimpleMixSplit>;
//    using BloomFilterFactory = filter::FilterAPI<BloomFilter>;
//    using BloomFilterPtr = std::shared_ptr<BloomFilter>;
    using Cache = lru11::Cache<Key, Value>;
    using CachePtr = std::shared_ptr<Cache>;

    using ServerPtr = std::unique_ptr<rpc::server>;
    using ClientPtr = std::unique_ptr<rpc::client>;
    String host;
    UInt64 port;
    ServerPtr srv;
    UInt32 worker_num;
    UInt32 slot_num;
    std::vector<ClientPtr> clients;

    UInt64 vertex_num;
//    std::vector<BloomFilterPtr> filters;
    std::vector<CachePtr> caches;

    std::vector<InsertionPool> pools;
    UInt32 pipeline_level;

    UInt64 index{0};

    bool finished{false};

private:
    inline static bool isFull(InsertionPool &pool) {
        const UInt32 POOL_SIZE{64u << 20}; // 64MB
        const auto pool_capacity = POOL_SIZE / sizeof(Key); // TODO:
        return pool.size() == pool_capacity;
    }

    void flushPool(UInt32 worker_id, InsertionPool &pool) {
        // TODO: Maybe finish.
        Stopwatch w;
        auto &c = clients[worker_id];
        __gnu_parallel::sort(pool.begin(), pool.end(), [](const InsertPack &lhs, const InsertPack &rhs) {
            return std::get<2>(lhs) < std::get<2>(rhs);
        });
#ifdef RPCLIB_DEBUG
        std::cout << "[Server] bulkInsert: Pool is sorted.\n";
#endif
//        auto ft = c->async_call("bulkInsert", pool);
//        ft.wait();
//        auto bulkInsert = [&](std::vector<InsertPack> &data, UInt32 pipeline_level_) {
//            // FIXME
//            const size_t NUM = 64;
//            using KBuff = std::array<InsertPack, NUM>;
//            std::vector<KBuff> buffs(pipeline_level_);
//            auto unit_size = NUM * pipeline_level_;
//            auto total_num = data.size();
//            total_num = total_num / unit_size + (total_num % unit_size != 0);
//            for (auto i = 0u; i < total_num; ++i) {
//                for (auto j = 0u; j < pipeline_level_; ++j) {
//                    auto b = i * unit_size + j * NUM;
//                    auto e = b + NUM;
//                    if (e >= data.size()) {
//                        e = data.size();
//                    }
//                    if (e != data.size()) {
//                        auto &buff = buffs[j];
//                        // TODO: optimize
//                        std::copy(data.begin() + b, data.begin() + e, buff.begin());
//                        c->send("bulkInsert", buff);
//                    } else {
//                        std::vector<InsertPack> tail_buff(data.begin() + b, data.end());
//                        c->send("bulkInsert", tail_buff);
//                        break;
//                    }
//
//                }
//                if (i % 1000 == 0)
//                    c->wait_all_responses();
//            }
//        };
//        bulkInsert(pool, pipeline_level);
        c->send("bulkInsert", pool);
        pool.clear();
        std::cout << "Flush Pool(" << worker_id << ") [" << w.elapsedMilliseconds() << "ms]\n";
    }

    inline auto assignKey(Key key) {
        auto hash_value = hashKey(key);
//        UInt32 worker_id = fastrange64(hash_value, worker_num);
//        UInt32 slot_id = fastrange64(hash_value, slot_num);
        UInt32 worker_id = hash_value % worker_num;
        UInt32 slot_id = hash_value % slot_num;
        slot_id = slot_id / worker_num;
        if (slot_id >= slot_num / worker_num) {
            std::cout << "ASSIGN KEY ERROR: " << hash_value << ", " << worker_id << ", " << (hash_value % slot_num)
                      << ", " << slot_id << "\n";
        }
        return std::make_tuple(worker_id, slot_id);
    }

public:
    DHIServer(String host_, UInt64 port_, UInt32 worker_num_, UInt32 slot_num_, UInt64 vertex_num_, UInt32 pipeline_level_,
              Float64 cache_coefficient = 0.1, Float64 elasticity_coefficient = 0.2)
            : host(host_), port(port_), worker_num(worker_num_), slot_num(slot_num_), vertex_num(vertex_num_), pipeline_level(pipeline_level_) {
        try {
            srv = std::make_unique<rpc::server>(host, port);
            clients.resize(worker_num);
            // FIXME
            clients[0] = std::make_unique<rpc::client>("20.0.1.119", 13333);
            clients[1] = std::make_unique<rpc::client>("20.0.1.121", 13333);
            clients[2] = std::make_unique<rpc::client>("20.0.1.124", 13333);
            for (auto &c: clients) {
                c->call("reset");
            }

            caches.resize(worker_num);
            // Assign each key to different worker
            UInt64 cache_max_size = vertex_num / worker_num * cache_coefficient;
            UInt64 cache_elasticity = cache_max_size * elasticity_coefficient;
            for (auto &fp: caches) {
                fp = std::make_shared<Cache>(cache_max_size, cache_elasticity);
            }

            pools.resize(worker_num);
            for (auto &pool : pools) {
                pool.reserve(pool_capacity);
                assert(pool.capacity() == pool_capacity);
            }
#ifdef RPCLIB_DEBUG
            std::cerr << "Create RPC Server (" << host << ":" << port << ")\n";
#endif
        }
        catch (std::exception &e) {
            std::cerr << "Create RPC Server (" << host << ":" << port << ") error: " << e.what() << "\n";
        }
    }

    void registerRPCServices() {
        try {
            // TODO: String key, Murmur Hash or CityHash
            //    srv->bind("insert", [&](String key) {
            srv->bind("insert", [&](Key key) {
                if (finished) {
                    std::cerr << "DHI is finished. New insertions are forbidden.\n";
                    return false;
                }
                auto[wid, sid] = assignKey(key);
                auto &cache = caches[wid];
                Value ph;
                if (!cache->tryGet(key, ph)) {
                    cache->insert(key, index);
                    auto &pool = pools[wid];
                    pool.emplace_back(key, index, sid);
                    index++;
                    if (isFull(pool))
                        flushPool(wid, pool);
                }
                return true;
            });

            srv->bind("bulkInsert", [&](const std::vector<Key> &keys) {
                if (finished) {
                    std::cerr << "DHI is finished. New insertions are forbidden.\n";
                    return false;
                }
                Stopwatch w;
                for (auto key: keys) {
                    auto[wid, sid] = assignKey(key);
                    auto &cache = caches[wid];
                    Value ph;
                    if (!cache->tryGet(key, ph)) {
                        cache->insert(key, index);
                        auto &pool = pools[wid];
                        pool.emplace_back(key, index, sid);
                        index++;
                        if (isFull(pool))
                            flushPool(wid, pool);
                    }
                }
//                std::cout << "bulkInsert keys [" << w.elapsedMilliseconds() << "ms]\n";
                return true;
            });

            srv->bind("getIndex", [&](Key key) {
                if (!finished) {
                    std::cerr << "DHI is finished. New insertions are forbidden.";
                    return ERROR_INDEX;
                }
                [[maybe_unused]]auto[wid, sid] = assignKey(key);
                const auto &cache = caches[wid];
                Value ret;
                if (cache->tryGet(key, ret))
                    return ret;
                else {
                    // TODO: query remote hash table
                    auto ft_v = clients[wid]->async_call("get", key, sid);
                    ft_v.wait();
                    return ft_v.get().template as<ValueT>();
                }

            });

            srv->bind("finish", [&]() {
                for (auto i = 0u; i < pools.size(); ++i) {
                    auto &pool = pools[i];
                    if (!pool.empty())
                        flushPool(i, pool);
                }
                finished = true;
            });

            srv->bind("reset", [&] {
                finished = false;
            });
        }
        catch (const std::exception &e) {
            std::cerr << "RPC Server Bind error: " << e.what() << "\n";
        }
//        srv->async_run(worker_num);
        srv->run();
#ifdef RPCLIB_DEBUG
        std::cerr << " $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ starting servicing \n";
#endif
    }
};

int main(int argc, char *argv[]) {
    String host;
    UInt64 port;
    UInt32 worker_num;
    UInt32 slot_num;
    UInt64 vertex_num;
    UInt32 pipeline_level{40};
    Args::CmdLine cmd(argc, argv);
    {
        Args::Arg host_('h', "host", true, false);
        Args::Arg port_('p', "port", true, false);
        Args::Arg worker_num_('n', "worker_num", true, false);
        Args::Arg slot_num_('s', "slot_num", true, false);
        Args::Arg vertex_num_('v', "vertex_num", true, false);
        Args::Arg pipeline_level_('l', "pipeline_level", true, false);
        cmd.addArg(host_);
        cmd.addArg(port_);
        cmd.addArg(worker_num_);
        cmd.addArg(slot_num_);
        cmd.addArg(vertex_num_);
        cmd.addArg(pipeline_level_);
        cmd.parse();
        host = host_.isDefined() ? host_.value() : "0.0.0.0";
        port = std::stoul(port_.isDefined() ? port_.value() : "23333");
        worker_num = std::stoi(worker_num_.isDefined() ? worker_num_.value() : "3");
        slot_num = std::stoi(slot_num_.isDefined() ? slot_num_.value() : "30");
        vertex_num = std::stoi(vertex_num_.isDefined() ? vertex_num_.value() : "50000000");
        pipeline_level = std::stoul(pipeline_level_.isDefined() ? pipeline_level_.value() : "40");
    }
    DHIServer server(host, port, worker_num, slot_num, vertex_num, pipeline_level);
    server.registerRPCServices();
    std::cout << "Starting Serving\n";
    return 0;
}