//
// Created by 赵程 on 2021/4/22.
//

#include <atomic>
#include <vector>
#include <iostream>
#include <thread>

#include <rpc/server.h>
#include <args-parser/Args/all.hpp>

#include <utils/Gang.h>

#include "Common.h"

#define CONTAIN_ATTRIBUTES  __attribute__ ((noinline))

template<typename Key=KeyT, typename Value=ValueT>
class DHICoordinatorServer {
    using ServerPtr = std::unique_ptr<rpc::server>;
    String host;
    UInt64 port;
    ServerPtr srv;
    const UInt32 worker_id;
    UInt32 slot_num;

    using Map = std::unordered_map<Key, Value>;
    using MapPtr = std::shared_ptr<Map>;
    std::vector<MapPtr> maps;

    GangUtil gu{};

public:
    DHICoordinatorServer(String host_, UInt64 port_, UInt32 worker_id_, UInt32 slot_num_)
            : host(host_), port(port_), worker_id(worker_id_), slot_num(slot_num_) {
        try {
            srv = std::make_unique<rpc::server>(host, port);

            maps.resize(slot_num);
            for (auto &map: maps) {
                map = std::make_shared<Map>();
            }

#ifdef RPCLIB_DEBUG
            std::cerr << "Create RPC Server (" << host << ":" << port << ")\n";
            std::cerr << "Create slot maps. size: (" << maps.size() << ")\n";
#endif
        }
        catch (std::exception &e) {
            std::cerr << "Create RPC Server (" << host << ":" << port << ") error: " << e.what() << "\n";
        }
    }

    void registerRPCServices() {
        try {
            // TODO: String key, Murmur Hash or CityHash
            srv->bind("insert", [&](Key k, Value v, UInt32 sid) -> bool {
                MapPtr &map = maps[sid];
                if (map->contains(k))
                    return false;
                (*map)[k] = v;
                return true;
            });

            srv->bind("insertPack", [&](InsertPack ip) -> bool {
                auto&[k, v, sid] = ip;
                MapPtr &map = maps[sid];
                if (map->contains(k))
                    return false;
                (*map)[k] = v;
                return true;
            });

            srv->bind("bulkInsert", [&](InsertionPool &pool) -> void {
                Stopwatch w;
                std::vector<UInt64> offs(slot_num);
                for (auto i = 1u; i < slot_num; ++i) {
                    InsertPack ip_ph{0, 0, i};
                    offs[i - 1] = std::lower_bound(pool.begin(), pool.end(), ip_ph,
                                                   [](const InsertPack &lhs, const InsertPack &rhs) {
                                                       return std::get<2>(lhs) < std::get<2>(rhs);
                                                   }) - pool.begin();
                }
                offs.back() = pool.size();
#ifdef RPCLIB_DEBUG
                std::cout << "[Coordinator] bulkInsert: Slot offsets: {";
                for (auto &off: offs) {
                    std::cout << " " << off;
                }
                std::cout << " }\n";
                std::cout << "sample sorted pool: \n";
                for (auto i = 0u; i < slot_num; ++i) {
                    std::cout << "  #### " << i << ":";
                    for (auto j = slot_num; j; --j) {
                        std::cout << " " << std::get<2>(pool[offs[i] - j]);
                    }
                    std::cout << "\n";
                }
#endif

                gu.groupSubmit(slot_num, [&](auto i) {
                    auto b = i? offs[i-1]: 0;
                    auto e = offs[i];
                    for (; b < e; ++b) {
                        auto&[k, v, sid] = pool[b];
                        assert(sid == i);
                        MapPtr &map = maps[sid];
                        if (map->contains(k))
                            continue;
                        (*map)[k] = v;
                    }
                });
//                std::cout << "Bulk Inserted (" << pool.size() << ") in Coordinator[" << worker_id << "]. ["
//                          << w.elapsedMilliseconds() << "ms]\n";
            });

            srv->bind("get", [&](Key k, UInt32 sid) -> Value {
                MapPtr &map = maps[sid];
                if (map->contains(k))
                    return (*map)[k];
                else {
                    std::cerr << "Dose not contain " << k << "\n";
//                    throw "Does not contain " + std::to_string(k);
                    return -1ull;
                }
            });

            srv->bind("reset", [&]() {
                for (auto &map: maps) {
                    map->clear();
                }
            });

        }
        catch (const std::exception &e) {
            std::cerr << "RPC Server Bind error: " << e.what() << "\n";
        }
//        srv->async_run(1);
        srv->run();
#ifdef RPCLIB_DEBUG
        std::cerr << " $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ starting servicing \n";
#endif
    }
};

int main(int argc, char *argv[]) {
    String host;
    UInt64 port;
    UInt32 worker_id;
    UInt32 slot_num;
    Args::CmdLine cmd(argc, argv);
    {
        Args::Arg host_('h', "host", true, false);
        Args::Arg port_('p', "port", true, false);
        Args::Arg worker_id_('i', "worker", true, true);
        Args::Arg slot_num_('s', "slot_num", true, false);
        cmd.addArg(host_);
        cmd.addArg(port_);
        cmd.addArg(worker_id_);
        cmd.addArg(slot_num_);
        cmd.parse();
        host = host_.isDefined() ? host_.value() : "0.0.0.0";
        port = std::stoul(port_.isDefined() ? port_.value() : "13333");
        worker_id = std::stoi(worker_id_.value());
        slot_num = std::stoi(slot_num_.isDefined() ? slot_num_.value() : "10");
    }
    DHICoordinatorServer server(host, port, worker_id, slot_num);
    server.registerRPCServices();
    return 0;
}