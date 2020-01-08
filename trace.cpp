#include "trace.h"
#include <cassert>
#include <windows.h>
#include <map>

//-------------------------------------------------------------------------------------------------------------------------------------------
typedef SYSTEMTIME time;
static void get_time(
	_out time &time
) {
	Winapi::GetLocalTime(&time);
}
static time get_time(
) {
	time time;
	get_time(time);
	return time;
}

namespace string {
	bool is__null_or_empty(_in cstr_t str) {
		if (!str)
			return true;
		if (L'\0' == *str)
			return true;
		return false;
	}
}

//-------------------------------------------------------------------------------------------------------------------------------------------
/*static*/ string_t tracer::settings::get_module(
	_in bool without_extension /*= false*/
) {
	// QueryFullProcessImageNameW
	// GetProcessImageFileNameW
	// GetModuleFileNameW/GetModuleFileNameExW
	
	std::pair<std::unique_ptr<char_t>, unsigned> buffer;
	for (buffer.second = 10; ; buffer.second += 64) {

		buffer.first.reset(new char_t[buffer.second]);
		const auto Size = Winapi::GetModuleFileNameW(NULL, buffer.first.get(), buffer.second);
		if (stdex::is__in_range(Size, {1, buffer.second})) {
			buffer.second = Size;
			break;
		}
		const auto LastError = Winapi::GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER != LastError)
			throw LastError;
	}

	std::pair<cstr_t, cstr_t> path = std::make_pair(buffer.first.get(), buffer.first.get() + buffer.second);
	{
		decltype(path) found = { nullptr, nullptr };
		for (cstr_t p = path.second; --p != path.first; ) {
			switch (*p) {
			case L'.':
				if (!found.second)
					found.second = p;
				break;
			case L'\\':
				if (!found.first) {
					found.first = p;
					goto go_next;
				}
				break;
			}
		}

	go_next:
		if (found.first)
			path.first = found.first + 1;
		if (without_extension && found.second)
			path.second = found.second;
	}

	return { path.first, path.second };
}

tracer::settings::settings(
	_in const config &config
) :
	outputs(config.outputs), show_flags(config.show_flags)
{
	module = ::string::is__null_or_empty(config.module) ? get_module() : config.module;
	if (::string::is__null_or_empty(config.file.path) && outputs.check(trace::output::file) ) {

		std::ios_base::openmode openmode = std::ios_base::out;		// auto doesn't work
		if (config.file.openmode__is_append)
			openmode |= std::ios_base::app | std::ios_base::ate;
		file.open(config.file.path, openmode);
		assert(file.is_open());

		if (config.file.openmode__is_append && (0 < file.tellp()))
			file.put(L'\n');
	}
}

//-------------------------------------------------------------------------------------------------------------------------------------------
tracer::string::string(
	_in unsigned size /*= 4096*/
) :
	_size(0)
{
	_buffer.data.reset(new char_t[size]);
	assert(_buffer.data.get());
	_buffer.size = size;
}

//unsigned tracer::string::format(
//	_in unsigned offset, _in cstr_t format, _in ...
//) {
//	va_list args;
//	va_start(args, format);
//	const auto result = string::format(offset, format, args);
//	va_end(args);
//	return result;
//}
unsigned tracer::string::format(
	_in unsigned offset, _in cstr_t format, _in va_list args
) {
	assert(_size < _buffer.size);
	auto result = vswprintf_s(offset + _buffer.data.get(), _buffer.size - offset, format, args);
	if (0 > result) {
		// ...
		result = 0;
	}
	return result;
}

unsigned tracer::string::format(
	_in cstr_t format, _in va_list args
) {
	_size = string::format(0, format, args);
	return _size;
}
unsigned tracer::string::format_append(
	_in cstr_t format, _in va_list args
) {
	_size += string::format(_size, format, args);
	return _size;
}

unsigned tracer::string::format(
	_in cstr_t format, _in ...
) {
	va_list args;
	va_start(args, format);
	const auto result = string::format(format, args);
	va_end(args);
	return result;
}
unsigned tracer::string::format_append(
	_in cstr_t format, _in ...
) {
	va_list args;
	va_start(args, format);
	const auto result = string::format_append(format, args);
	va_end(args);
	return result;
}

unsigned tracer::string::append(
	_in char_at c
) {
	assert(_size < _buffer.size);
	if (_size + 1 != _buffer.size) {
		_buffer.data.get()[_size++] = L'\n';
		_buffer.data.get()[_size] = L'\0';
	}
	return _size;
}

constexpr unsigned tracer::string::size(
) const {
	return _size;
}
cstr_t tracer::string::c_str() const {
	return _buffer.data.get();
}

//-------------------------------------------------------------------------------------------------------------------------------------------
tracer::tracer(
	_in const config &config
) :
	_settings(config)
{
	const time &time = get_time();
	//const auto pid = Winapi::GetCurrentProcessId();													// without pid(dec)

	// == <<|2020-01-09 0:15:45.942| ===========================================================================================
	//_string.format(L"%s %s|%04hu-%02hu-%02hu %hu:%02hu:%02hu.%03hu|0x%X(%u)| %s\n", 
	_string.format(L"%s %s|%04hu-%02hu-%02hu %hu:%02hu:%02hu.%03hu|0x%X| %s\n",							// without pid(dec)
		std::wstring(_padding.line_separator.count.first, _padding.line_separator.ch).c_str(),
		std::wstring(_padding.ctor_dtor.count, _padding.ctor_dtor.ch.first).c_str(),
		time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, 
		Winapi::GetCurrentProcessId(),	//pid, pid,														// without pid(dec)
		std::wstring(_padding.line_separator.count.second, _padding.line_separator.ch).c_str()
	);
	output();
}
tracer::~tracer(
) {
	const time &time = get_time();
	//const auto pid = Winapi::GetCurrentProcessId();													// without pid(dec)

	// == >>|2020-01-09 0:15:45.942| ===========================================================================================
	//_string.format(L"%s %s|%04hu-%02hu-%02hu %hu:%02hu:%02hu.%03hu|0x%X(%u)| %s\n",
	_string.format(L"%s %s|%04hu-%02hu-%02hu %hu:%02hu:%02hu.%03hu|0x%X| %s\n",							// without pid(dec)
		std::wstring(_padding.line_separator.count.first, _padding.line_separator.ch).c_str(),
		std::wstring(_padding.ctor_dtor.count, _padding.ctor_dtor.ch.second).c_str(),
		time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
		Winapi::GetCurrentProcessId(),	//pid, pid,														// without pid(dec)
		std::wstring(_padding.line_separator.count.second, _padding.line_separator.ch).c_str()
	);
	output();
}

unsigned tracer::trace(
	_in cstr_t function /*= __FUNCTIONW__*/, _in trace::categoty category, _in cstr_t format, _in va_list args
) {
	string__create_prefix(function);
	string__append_userdata(category, format, args);

	output();

	return _string.size() /*- 1*/;
}
unsigned tracer::trace(
	_in cstr_t function /*= __FUNCTIONW__*/, _in trace::categoty category, _in cstr_t format, ...
) {
	va_list args;
	va_start(args, format);
	const auto result = trace(function, category, format, args);
	va_end(args);
	return result;
}

//void tracer::string__append_n(
//) {
//	_string.append(L'\n');
//}

void tracer::string__create_prefix(
	_in cstr_t function /*= __FUNCTIONW__*/
) {
	const time &time = get_time();
	_string.format(L"%hu:%02hu:%02hu.%03hu|%s|", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, _settings.module.c_str());
	if (_settings.show_flags.check(trace::show_flag::process_id)) {
		//const auto pid = Winapi::GetCurrentProcessId();
		//_string.format_append(L"0x%X(%u):", pid, pid);
		_string.format_append(L"0x%X:", Winapi::GetCurrentProcessId());
	} {
		//const auto tid = Winapi::GetCurrentThreadId();
		//_string.format_append(L"0x%X(%u)|", tid, tid);
		_string.format_append(L"0x%X|", Winapi::GetCurrentThreadId());
	}
	//_string.format_append(L"%s()|", function);
	_string.format_append(L"%s|", function);
}
void tracer::string__append_userdata(
	_in trace::categoty category, _in cstr_t format, _in va_list args
) {
	static const std::map<trace::categoty, char_t> map_tc{
		{trace::categoty::normal,  L'N'},
		{trace::categoty::warning, L'W'},
		{trace::categoty::error,   L'E'},
	};
	_string.format_append(L"%c|", map_tc.at(category));

	std::wstring format_new(format);
	format_new.push_back(L'\n');
	_string.format_append(format_new.c_str(), args);
}

void tracer::output(
) const {
	if (_settings.outputs.check(trace::output::monitor))
		output_to__monitor();
	if (_settings.outputs.check(trace::output::file))
		output_to__file();
}

void tracer::output_to__monitor(
) const {
	Winapi::OutputDebugStringW(_string.c_str());
}
void tracer::output_to__file(
) const {
	if (_settings.file.is_open())
		_settings.file.write(_string.c_str(), _string.size());
}

//-------------------------------------------------------------------------------------------------------------------------------------------
