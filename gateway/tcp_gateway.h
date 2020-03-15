#pragma once

#include "tcp_common.h"
#include "tcp_link.h"
class tcp_link;

class tcp_gateway : public tcp_event_gateway
{
public:
	tcp_gateway() {}
	~tcp_gateway() {}

protected:
	struct  close_info
	{
		shared_ptr<tcp_link> _tcp_link = nullptr;
		int _close_times = 0;
	};
	std::mutex m_mtx;
	tcp_server m_tcp_server;
	map<shared_ptr<tcp_client>, shared_ptr<tcp_link>> tcp_link_map;
	map<shared_ptr<tcp_client>, shared_ptr<tcp_link>> tcp_link_map_wait;
	shared_ptr<tcp_link> tcp_link_temp = nullptr;

public:
	bool start(const std::string& host, std::uint32_t port)
	{
		try
		{
			m_tcp_server.start(host, port, std::bind(&tcp_gateway::on_new_connection, this, std::placeholders::_1));
		}
		catch (tacopie_error& e)
		{
			cout << e.what() << endl;

			return false;
		}

		return true;
	}

	bool stop()
	{
		try
		{
			m_tcp_server.stop(true);
		}
		catch (tacopie_error& e)
		{
			cout << e.what() << endl;

			return false;
		}

		return true;
	}

	int get_client_num()
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		return (int)tcp_link_map.size();
	}
private:
	bool on_new_connection(const shared_ptr<tcp_client>& _tcp_cli)
	{
		std::unique_lock<std::mutex> lock(m_mtx);
		shared_ptr<tcp_link>& _tcp_link = tcp_link_map[_tcp_cli];
		if (_tcp_link == nullptr)
		{
			tcp_link_map[_tcp_cli] = std::make_shared<tcp_link>();
		}
		lock.unlock();

		_tcp_link->on_connect(this, _tcp_cli);

		return true;
	}

	bool on_del_connection(const shared_ptr<tcp_client>& _tcp_cli)
	{
		assert(_tcp_cli != nullptr);
		std::lock_guard<std::mutex> lock(m_mtx);

		tcp_link_map.erase(_tcp_cli);

		return true;
	}

};