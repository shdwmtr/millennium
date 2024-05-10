#pragma once
#include <vector>
#include <Python.h>
#include <thread>
#include <functional>
#include <string>
#include <generic/stream_parser.h>

struct thread_state {
	std::string name;
	PyThreadState* thread_state;
	PyThreadState* auxts;
};

class plugin_manager 
{
private:
	PyThreadState* _save;
	short instance_count;

	std::vector<std::thread> thread_pool;
	std::vector<thread_state> instances_;

public:
	plugin_manager();
	~plugin_manager();

	bool create_instance(stream_buffer::plugin_mgr::plugin_t& plugin, std::function<void(stream_buffer::plugin_mgr::plugin_t&)> callback);
	void wait_for_exit();

	thread_state get_thread_state(std::string pluginName);
	std::string get_plugin_name(PyThreadState* thread);

	bool shutdown_plugin(std::string plugin_name);

	static plugin_manager& get() {
		static plugin_manager instance;
		return instance;
	}
};