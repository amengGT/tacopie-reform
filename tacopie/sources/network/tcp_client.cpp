// MIT License
//
// Copyright (c) 2016-2017 Simon Ninon <simon.ninon@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <tacopie/network/tcp_client.hpp>
#include <tacopie/utils/error.hpp>
#include <tacopie/utils/logger.hpp>

namespace tacopie {

//!
//! ctor & dtor
//!

tcp_client::tcp_client(void)
: m_disconnection_handler(nullptr) {
  m_io_service = get_default_io_service();
  __TACOPIE_LOG(debug, "create tcp_client");
}

tcp_client::~tcp_client(void) {
  __TACOPIE_LOG(debug, "destroy tcp_client");
  
  //! update state
  m_is_connected = false;

  //! clear all pending requests  clear_read_requests();  clear_write_requests();

  //! remove socket from io service and wait for removal if necessary  m_io_service->untrack(m_socket);  m_io_service->wait_for_removal(m_socket);

  m_socket.close();
}

//!
//! custom ctor
//! build client from existing socket
//!

tcp_client::tcp_client(tcp_socket&& socket)
: m_io_service(get_default_io_service())
, m_socket(std::move(socket))
, m_disconnection_handler(nullptr) {
  m_is_connected = true;
  __TACOPIE_LOG(debug, "create tcp_client");
  m_io_service->track(m_socket);
}

//!
//! get host & port information
//!

const std::string&
tcp_client::get_host(void) const {
  return m_socket.get_host();
}

std::uint32_t
tcp_client::get_port(void) const {
  return m_socket.get_port();
}

//!
//! start & stop the tcp client
//!

void
tcp_client::connect(const std::string& host, std::uint32_t port, std::uint32_t timeout_msecs) {
  if (is_connected()) { __TACOPIE_THROW(warn, "tcp_client is already connected"); }

  try {
    m_socket.connect(host, port, timeout_msecs);
    m_io_service->track(m_socket);
  }
  catch (const tacopie_error& e) {
    m_socket.close();
    throw e;
  }

  m_is_connected = true;

  __TACOPIE_LOG(info, "tcp_client connected");
}

void
tcp_client::disconnect(bool force_disconnect) {
	
	bool notify_close = false;
	{
		std::lock_guard<std::mutex> lock(m_requests_mtx);
		//
		if (force_disconnect)notify_close = force_close();
		else notify_close = grace_close();
	}

	if(notify_close) call_disconnection_handler(9);
	return;
}

bool tcp_client::grace_close()
{
	//
	if (m_is_connected == false || m_is_shuting == true) return false;

	//
	m_is_shuting = true;

	if (m_is_sending == false && m_is_recving == true)
	{
		m_is_connected = false;
		m_io_service->untrack(m_socket);
		return false;
	}

	//close over
	if (m_is_sending == false && m_is_recving == false)
	{
		m_is_connected = false;

		//! clear all pending requests
		clear_read_requests();
		clear_write_requests();

		//! remove socket from io service and wait for removal if necessary
		m_io_service->untrack(m_socket);
		m_io_service->wait_for_removal(m_socket);

		m_socket.close();

		return true;
	}

	return true;
}

bool
tcp_client::force_close()
{
	//! update state
	if (m_is_connected)
	{
		m_is_connected = false;
		m_io_service->untrack(m_socket);
	}

	//close over
	if (m_is_sending == false && m_is_recving == false)
	{
		//! clear all pending requests
		clear_read_requests();
		clear_write_requests();

		//! remove socket from io service and wait for removal if necessary
		m_io_service->wait_for_removal(m_socket);

		m_socket.close();

		//notify shut
		return true;
	}

	return false;
}

//!
//! Clear pending requests
//!
void
tcp_client::clear_read_requests(void) {
  //std::lock_guard<std::mutex> lock(m_requests_mtx);

  std::queue<read_request> empty;
  std::swap(m_read_requests, empty);
}

void
tcp_client::clear_write_requests(void) {
  //std::lock_guard<std::mutex> lock(m_requests_mtx);

  std::queue<write_request> empty;
  std::swap(m_write_requests, empty);
}

//!
//! Call disconnection handler
//!
void
tcp_client::call_disconnection_handler(int code) {
	if (m_disconnection_handler) {
	  m_disconnection_handler(code);
  }
}

//!
//! io service read callback
//!

void
tcp_client::on_read_available(fd_t) {
  __TACOPIE_LOG(info, "read available");

  int code = 0;
  bool notify_close = false;
  read_result result;
  async_read_callback_t callback = nullptr;

  //process read
  {
	  std::lock_guard<std::mutex> lock(m_requests_mtx);

	  //check
	  assert(m_is_recving == true);
	  assert(!m_read_requests.empty());

	  m_is_recving = false;

	  if (m_is_connected == false)
	  {
		  code += 1;
		  notify_close = force_close();
		  goto read_notify;
	  }

	  const auto& request = m_read_requests.front();
	  callback = request.async_read_callback;

	  try {
		  result.buffer = m_socket.recv(request.size);
		  result.success = true;

		  m_read_requests.pop();

		  //check close
		  if (m_is_shuting == true)
		  {
			  code += 3;
			  notify_close = force_close();
		  }

		  //check read requests
		  else if (!m_read_requests.empty())
		  {
			  //post recv
			  m_io_service->set_rd_callback(m_socket, std::bind(&tcp_client::on_read_available, this, std::placeholders::_1));

			  m_is_recving = true;
		  }
	  }
	  catch (const tacopie::tacopie_error&) {
		  code += 2;
		  notify_close = force_close();
		  result.success = false;
	  }
  }

  //notify read result
  if (callback) { callback(result); }

  //notify close
read_notify:
  if (notify_close) call_disconnection_handler(code);
}

//!
//! io service write callback
//!

void
tcp_client::on_write_available(fd_t) {
  __TACOPIE_LOG(info, "write available");

  int code = 10;
  bool notify_close = false;
  write_result result;
  async_write_callback_t callback = nullptr;

  {
	  std::lock_guard<std::mutex> lock(m_requests_mtx);

	  //check
	  assert(m_is_sending == true);
	  assert(!m_write_requests.empty());

	  m_is_sending = false;

	  if (m_is_connected == false)
	  {
		  code += 1;
		  notify_close = force_close();
		  goto write_notify;
	  }

	  const auto& request = m_write_requests.front();
	  auto callback = request.async_write_callback;

	  try {
		  result.size = m_socket.send(request.buffer, request.buffer.size());
		  result.success = true;

		  m_write_requests.pop();

		  //check request
		  if (!m_write_requests.empty())
		  {
			  //post send
			  m_io_service->set_wr_callback(m_socket, std::bind(&tcp_client::on_write_available, this, std::placeholders::_1));

			  m_is_sending = true;
		  }
		  //check shuting
		  else if (m_is_shuting == true)
		  {
			  code += 3;
			  notify_close = force_close();
		  }
	  }
	  catch (const tacopie::tacopie_error&) {
		  code += 2;
		  notify_close = force_close();
		  result.success = false;
	  }
  }

  if (callback) { callback(result); }

write_notify:
  //notify close
  if (notify_close) call_disconnection_handler(code);
  
  return;
}

//!
//! process read & write operations when available
//!

tcp_client::async_read_callback_t
tcp_client::process_read(read_result& result) {
  std::lock_guard<std::mutex> lock(m_requests_mtx);

  //check
  assert(m_is_recving == true);
  assert(!m_read_requests.empty());

  m_is_recving = false;

  if (m_socket.get_fd() == __TACOPIE_INVALID_FD)
  {
	  result.success = false;
	  return nullptr;
  }

  if (m_read_requests.empty()) { return nullptr; }

  const auto& request = m_read_requests.front();
  auto callback       = request.async_read_callback;

  try {
    result.buffer  = m_socket.recv(request.size);
    result.success = true;
  }
  catch (const tacopie::tacopie_error&) {
    result.success = false;
  }

  m_read_requests.pop();

  return callback;
}

tcp_client::async_write_callback_t
tcp_client::process_write(write_result& result) {
  std::lock_guard<std::mutex> lock(m_requests_mtx);

  //check
  assert(m_is_sending == true);
  assert(!m_write_requests.empty());

  m_is_sending = false;

  if (m_socket.get_fd() == __TACOPIE_INVALID_FD)
  {
	  result.success = false;
	  return nullptr;
  }

  if (m_write_requests.empty()) { return nullptr; }

  const auto& request = m_write_requests.front();
  auto callback       = request.async_write_callback;

  try {
    result.size    = m_socket.send(request.buffer, request.buffer.size());
    result.success = true;
  }
  catch (const tacopie::tacopie_error&) {
    result.success = false;
  }

  m_write_requests.pop();

  return callback;
}

//!
//! async read & write operations
//!

void
tcp_client::async_read(const read_request& request) {

	//lock 
	std::unique_lock<std::mutex> lock(m_requests_mtx);

	//check close
	if (m_is_connected == false || m_is_shuting == true) return;

	//save request
	m_read_requests.push(request);

	//check
	if (m_is_recving) return;

	//post recv
	m_io_service->set_rd_callback(m_socket, std::bind(&tcp_client::on_read_available, this, std::placeholders::_1));

	m_is_recving = true;

	return;
}

void
tcp_client::async_write(const write_request& request) {
  //
  //lock 
  std::lock_guard<std::mutex> lock(m_requests_mtx);

	//check close
	if (m_is_connected == false || m_is_shuting == true) return;

	//save request
	m_write_requests.push(request);

	//check
	if (m_is_sending) return;

	//post send
	m_io_service->set_wr_callback(m_socket, std::bind(&tcp_client::on_write_available, this, std::placeholders::_1));

	m_is_sending = true;

	return;
}

//!
//! socket getter
//!

tacopie::tcp_socket&
tcp_client::get_socket(void) {
  return m_socket;
}

const tacopie::tcp_socket&
tcp_client::get_socket(void) const {
  return m_socket;
}

//!
//! io_service getter
//!
const std::shared_ptr<tacopie::io_service>&
tcp_client::get_io_service(void) const {
  return m_io_service;
}

//!
//! set on disconnection handler
//!

void
tcp_client::set_on_disconnection_handler(const disconnection_handler_t& disconnection_handler) {
  m_disconnection_handler = disconnection_handler;
}

//!
//! returns whether the client is currently running or not
//!

bool
tcp_client::is_connected(void) /*const*/ {
	std::lock_guard<std::mutex> lock(m_requests_mtx);
	return m_socket.get_fd() != __TACOPIE_INVALID_FD;
}

//!
//! comparison operator
//!
bool
tcp_client::operator==(const tcp_client& rhs) const {
  return m_socket == rhs.m_socket;
}

bool
tcp_client::operator!=(const tcp_client& rhs) const {
  return !operator==(rhs);
}

} // namespace tacopie
