
#include "tcp_common.h"
#include "tcp_link.h"
#include "tcp_gateway.h"
#include <signal.h>

std::uint32_t g_connect_time_out = 1000;
std::uint32_t g_local_port = 0;
std::uint32_t g_remote_port = 0;
std::string g_remote_host = "";

void signint_handler(int) {return;}

int main()
{
#ifdef _WIN32
	WSADATA wsaData = { 0 };
	int nErrCode = 0;
	nErrCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (0 != nErrCode)
	{
		return 0;
	}
#endif

	signal(SIGINT, &signint_handler);

	tacopie::get_default_io_service()->set_nb_workers(20);
	tcp_gateway gateway;
	
	do 
	{
		cout << "local_port:"; cin >> g_local_port;
		cout << "remote_host:"; cin >> g_remote_host;
		cout << "remote_port:"; cin >> g_remote_port;
	} while (false == gateway.start("0.0.0.0", g_local_port));

	cout << "gateway run successfully" << endl;

	string cmd;
	char ch = 0;
	while (true)
	{
		ch = getchar();
		if (ch != '\n')
		{
			cmd.append(1, ch);
			continue;
		}

		if (cmd == "quit") break;
		else if (cmd == "clear") system("cls");
		else if (cmd == "clients") printf("client numbers:%d\n", gateway.get_client_num());
		else if (cmd == "network")
		{
			cout << "local_port:" << g_local_port << endl;
			cout << "remote_host:" << g_remote_host << endl;
			cout << "remote_port:" << g_remote_port << endl;
		}

		cmd.clear();
	}

	gateway.stop();
	cout << "gateway stop successfully" << endl;

#ifdef _WIN32
	WSACleanup();
#endif

	return 0;
}