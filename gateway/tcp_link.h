#pragma once
#include "tcp_common.h"

#ifdef _DEBUG
#define DEBUG_CODE
#endif

class tcp_event_gateway
{
public:
	virtual bool on_del_connection(const shared_ptr<tcp_client>& _tcp_cli) = 0;
};

class tcp_link
{
public:
	tcp_link() {}
	~tcp_link() {}

private:
#ifdef DEBUG_CODE
	int m_close_times = 0;
	int m_close_sindex = 0;
	int m_close_stimes[3] = {0};
	int m_close_cindex = 0;
	int m_close_ctimes[3] = { 0 };
#endif

protected:
	tcp_event_gateway* m_gateway;

	std::recursive_mutex m_close_mtx;
	bool m_close_scli = false;
	bool m_close_ccli = false;

	bool m_route = false;
	int m_tcp_send_count_c = 0;
	int m_tcp_send_count_s = 0;

	shared_ptr<tcp_client> m_tcp_ccli = nullptr;
	shared_ptr<tcp_client> m_tcp_scli = nullptr;

	//callback function
public:
	void on_connect(tcp_event_gateway* _gateway, const shared_ptr<tcp_client>& _tcp_cli)
	{
		if (_tcp_cli == nullptr && _gateway != nullptr) return;
		if (m_tcp_ccli != nullptr && m_gateway != nullptr ) return;

		//set callback interface
		m_gateway = _gateway;

		//set variable
		m_tcp_ccli = _tcp_cli;
		m_tcp_ccli->set_on_disconnection_handler(std::bind(&tcp_link::on_disconnect_c, this, std::placeholders::_1));
		m_tcp_ccli->async_read({ TCP_BUFF_LEN,std::bind(&tcp_link::on_receive_c, this, std::placeholders::_1) });
	}

	void on_disconnect_s(int code)
	{
		//cout << "on_disconnect scli" << endl;
		std::unique_lock<std::recursive_mutex> lock(m_close_mtx);
#ifdef DEBUG_CODE
		m_close_stimes[m_close_sindex++] = code;
#endif
		m_close_scli = true;

		if (m_tcp_ccli == nullptr || m_close_ccli)
		{
#ifdef DEBUG_CODE
			++m_close_times;
#endif
			lock.unlock();
			m_gateway->on_del_connection(m_tcp_ccli);
		}
		else if (m_tcp_ccli && m_tcp_ccli->is_connected())
		{
			lock.unlock();
			m_tcp_ccli->disconnect(false);
		}
	}

	void on_disconnect_c(int code)
	{
		//cout << "on_disconnect ccli" << endl;
		std::unique_lock<std::recursive_mutex> lock(m_close_mtx);
#ifdef DEBUG_CODE
		m_close_ctimes[m_close_cindex++] = code;
#endif
		m_close_ccli = true;

		if (m_tcp_scli == nullptr || m_close_scli)
		{
#ifdef DEBUG_CODE
			++m_close_times;
#endif
			lock.unlock();
			m_gateway->on_del_connection(m_tcp_ccli);
		}
		else if (m_tcp_scli && m_tcp_scli->is_connected())
		{
			lock.unlock();
			m_tcp_scli->disconnect(false);
		}
	}

	bool on_receive_s(tcp_client::read_result& result)
	{
		if (false == result.success) return true;

		try
		{
			//string str(result.buffer.begin(), result.buffer.end());
			//vector<char> vchar(str.begin(), str.end());

			++m_tcp_send_count_c;
			m_tcp_ccli->async_write({ result.buffer, std::bind(&tcp_link::on_write_ok_c, this, std::placeholders::_1) });
			m_tcp_scli->async_read({ TCP_BUFF_LEN,std::bind(&tcp_link::on_receive_s, this, std::placeholders::_1) });
		}
		catch (tacopie_error& e)
		{
			cout << "on_receive_s:" << e.what() << endl;
		}
		return true;
	}

	bool on_receive_c(tcp_client::read_result& result)
	{
		if (false == result.success) return true;

		try
		{
			if (false == m_route)
			{
				try {
					m_tcp_scli = std::make_shared<tcp_client>();
					m_tcp_scli->connect(g_remote_host, g_remote_port, g_connect_time_out);
				}
				catch (const std::exception& e)
				{
					cout << e.what() << endl;

					m_tcp_scli.reset();
					m_tcp_ccli->disconnect(false);
					return false;
				}
				m_tcp_scli->set_on_disconnection_handler(std::bind(&tcp_link::on_disconnect_s, this, std::placeholders::_1));
				m_tcp_scli->async_read({ TCP_BUFF_LEN,std::bind(&tcp_link::on_receive_s, this, std::placeholders::_1) });
				m_route = true;
			}

			m_tcp_scli->async_write({ result.buffer, std::bind(&tcp_link::on_write_ok_s, this, std::placeholders::_1) });
			m_tcp_ccli->async_read({ TCP_BUFF_LEN,std::bind(&tcp_link::on_receive_c, this, std::placeholders::_1) });
		}
		catch (tacopie_error& e)
		{
			cout << "on_receive:" <<e.what() << endl;
		}
		return true;
	}

	bool on_write_ok_s(tcp_client::write_result& result)
	{
		return true;
	}

	bool on_write_ok_c(tcp_client::write_result& result)
	{
		return true;
	}

protected:
	bool create_route()
	{

		return true;
	}

	bool on_handler_pack_c()
	{
		return true;
	}

};