del chat_client.exe
del chat_server.exe
g++ server_main.cpp -o chat_server.exe -lws2_32
g++ client_main.cpp -o chat_client.exe -lws2_32
pause