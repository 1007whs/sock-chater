del chat_client.exe
del chat_server.exe
g++ chat_client.cpp -o chat_client.exe -lws2_32
g++ chat_server.cpp -o chat_server.exe -lws2_32
pause