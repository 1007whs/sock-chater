#include "sock.hpp"
#include <thread>

// 接收消息线程函数
void receive_messages(TCPClient &client)
{
    std::string msg;
    while (client.isConnected())
    {
        if (client.receive_data(msg))
        {
            ConsoleColor::set(ConsoleColor::YELLOW);
            std::cout << "\n"
                      << msg << std::endl;
            ConsoleColor::set(ConsoleColor::WHITE);
            std::cout << "请输入消息 (输入exit退出): ";
            std::cout.flush(); // 确保提示显示
        }
        else
        {
            break;
        }
    }
}

int main()
{
    setConsoleUTF8();
    ConsoleColor::set(ConsoleColor::YELLOW);
    std::cout << "=== 多人聊天客户端 ===" << std::endl;
    ConsoleColor::set(ConsoleColor::WHITE);

    std::string server_ip;
    int port;
    std::string nickname;

    // 获取用户输入
    std::cout << "请输入服务器IP: ";
    std::cin >> server_ip;
    std::cout << "请输入端口号: ";
    std::cin >> port;
    std::cin.ignore(); // 忽略换行符
    std::cout << "请输入你的昵称: ";
    std::getline(std::cin, nickname);

    // 创建并连接客户端
    TCPClient client(server_ip, port);
    if (!client.connect())
    {
        std::cerr << "连接服务器失败" << std::endl;
        return 1;
    }

    // 发送昵称
    if (!client.send_data("NICKNAME " + nickname))
    {
        std::cerr << "昵称发送失败，退出程序" << std::endl;
        client.disconnect();
        return 1;
    }

    // 启动接收消息线程
    std::thread receiver(receive_messages, std::ref(client));
    receiver.detach(); // 分离线程

    // 主线程处理输入
    std::string input;
    std::cout << "连接成功！请输入消息 (输入exit退出): ";
    while (client.isConnected())
    {
        std::getline(std::cin, input);

        if (client.send_data(input))
        {
            if (input == "exit")
            {
                break;
            }
            // 每次发送消息后重新显示提示
            std::cout << "请输入消息 (输入exit退出): ";
        }
        else
        {
            break;
        }
    }

    client.disconnect();
    std::cout << "已退出聊天" << std::endl;
    return 0;
}
