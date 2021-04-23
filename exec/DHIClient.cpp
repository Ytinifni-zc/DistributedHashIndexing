//
// Created by 赵程 on 2021/4/21.
//

#include <iostream>
#include <numeric>

#include <rpc/client.h>
#include <args-parser/Args/all.hpp>

#include <utils/StopWatch.h>

#include "Common.h"

int main(int argc, char *argv[]) {
    // FIXME

    String server_host;
    UInt64 server_port;
    auto vertex_num = 10;
    bool checkResult;
    UInt32 pipeline_level{40};
    Args::CmdLine cmd(argc, argv);
    {
        Args::Arg host_('h', "host", true, false);
        Args::Arg port_('p', "port", true, false);
        Args::Arg vn_('v', "vertex_num", true, false);
        Args::Arg check_('c', "check", false, false);
        Args::Arg pipeline_level_('l', "pipeline_level", true, false);
        cmd.addArg(host_);
        cmd.addArg(port_);
        cmd.addArg(vn_);
        cmd.addArg(check_);
        cmd.addArg(pipeline_level_);
        cmd.parse();
        server_host = host_.isDefined() ? host_.value() : "20.0.1.121";
        server_port = std::stoul(port_.isDefined() ? port_.value() : "23333");
        if (vn_.isDefined())
            vertex_num = std::stoul(vn_.value());
        checkResult = check_.isDefined();
        pipeline_level = std::stoul(pipeline_level_.isDefined() ? pipeline_level_.value() : "40");
    }

    using ClientPtr = std::shared_ptr<rpc::client>;
    ClientPtr client = std::make_shared<rpc::client>(server_host, server_port);
    Stopwatch w;
    client->call("reset");
    std::cout << "Reset Server use: " << w.elapsedMilliseconds() << "ms\n";
    w.restart();
    std::vector<KeyT> keys(vertex_num);
    std::iota(keys.begin(), keys.end(), 0);
    std::cout << "Init keys use: " << w.elapsedMilliseconds() << "ms\n";
    w.restart();

    auto bulkInsert = [&](std::vector<KeyT> &data, UInt32 pipeline_level) {
        const size_t NUM = 1024 / sizeof(KeyT);
        using KBuff = std::array<KeyT, NUM>;
        std::vector<KBuff> buffs(pipeline_level);
        auto unit_size = NUM * pipeline_level;
        auto total_num = data.size();
        total_num = total_num / unit_size + (total_num % unit_size != 0);
        for (auto i = 0u; i < total_num; ++i) {
            for (auto j = 0u; j < pipeline_level; ++j) {
                auto b = i * unit_size + j * NUM;
                auto e = b + NUM;
                if (e >= data.size()) {
                    e = data.size();
                }
                if (e != data.size()) {
                    auto &buff = buffs[j];
                    // TODO: optimize
                    std::copy(data.begin() + b, data.begin() + e, buff.begin());
                    client->send("bulkInsert", buff);
                } else {
                    std::vector<KeyT> tail_buff(data.begin() + b, data.end());
                    client->send("bulkInsert", tail_buff);
                    break;
                }

            }
        }
    };

    bulkInsert(keys, pipeline_level);

    client->send("bulkInsert", keys);
    client->wait_all_responses();
    client->call("finish");
    std::cout << "Insert " << vertex_num << " keys using: " << w.elapsedMilliseconds() << "ms\n";

    if (!checkResult)
        return 0;
    auto interval = vertex_num / 100;
    for (auto i = 0ul; i < vertex_num; i += interval) {
        auto ft_index = client->async_call("getIndex", i);
        ft_index.wait();
        auto index = ft_index.get().as<ValueT>();
        std::cout << i << " -> " << index << "\n";
    }

    return 0;
}