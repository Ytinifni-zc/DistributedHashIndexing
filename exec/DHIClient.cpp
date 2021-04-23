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
    Args::CmdLine cmd(argc, argv);
    {
        Args::Arg host_('h', "host", true, false);
        Args::Arg port_('p', "port", true, false);
        Args::Arg vn_('v', "vertex_num", true, false);
        Args::Arg check_('c', "check", false, false);
        cmd.addArg(host_);
        cmd.addArg(port_);
        cmd.addArg(vn_);
        cmd.addArg(check_);
        cmd.parse();
        server_host = host_.isDefined() ? host_.value() : "20.0.1.121";
        server_port = std::stoul(port_.isDefined() ? port_.value() : "23333");
        if (vn_.isDefined())
            vertex_num = std::stoul(vn_.value());
        checkResult = check_.isDefined();
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
    client->call("bulkInsert", keys);
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