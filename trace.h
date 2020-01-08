#pragma once

#include "types.h"
#include <fstream>
#include <initializer_list>

namespace trace {
	enum class categoty {
		normal /*information*/ = 0, 
		warning, 
		error
	};
	enum class output {
		monitor = 1,												// debug-monitor, see Winapi::OutputDebugStringW()
		file = 2/*, console*/										// file on disk
	};
	enum class show_flag {
		breaf_info = 1,												// output breaf-info about current process in c_tor()
		process_id = 2												// output process-id in each trace output
	};
}

class tracer {
public:
	struct config {
		_optional cstr_t module;									// если не указано, будет использоваться имя файла образа текущего процесса
		struct file {
			_optional cstr_t path;									// если не указано, трассировка в файл выводиться не будет
			bool openmode__is_append;								// если true, то файл будет открыт в режиме доьавления новых трасс, иначе - содержимое будет
		};
		struct file file;
		
		std::initializer_list<trace::output> outputs;
		std::initializer_list<trace::show_flag> show_flags;
	};

	tracer(_in const config &config);
	~tracer();
	unsigned trace(_in cstr_t function /*= __FUNCTIONW__*/, _in trace::categoty category, _in cstr_t format, ...);

protected:
	class settings {
	public:
		template <typename type> class mask {
		public:
			mask(_in std::initializer_list<type> items): _mask(0) {
				for (const auto item : items)
					_mask |= static_cast<decltype(_mask)>(item);
			}
			bool check(_in type item) const {
				return _mask & static_cast<decltype(_mask)>(item);
			}
		private:
			unsigned _mask;
		};
	public:
		string_t module;
		mutable std::wofstream file;
		mask<trace::output> outputs;
		mask<trace::show_flag> show_flags;
	public:
		settings(_in const config &config);
	private:
		static string_t get_module(_in bool without_extension = false);
	};

	class string {
	protected:
		struct buffer {
			std::unique_ptr<char_t> data;
			unsigned size;
		} _buffer;
		unsigned _size;

	public:
		string(_in unsigned size = 4096);

		constexpr unsigned size() const;
		cstr_t c_str() const;

	public:
		unsigned format(_in cstr_t format, _in va_list args);								// возвращает size
		unsigned format_append(_in cstr_t format, _in va_list args);						// возвращает size
		unsigned format(_in cstr_t format, _in ...);										// возвращает size
		unsigned format_append(_in cstr_t format, _in ...);									// возвращает size

		unsigned append(_in char_at c);														// возвращает size

	protected:
		//unsigned format(_in unsigned offset, _in cstr_t format, _in ...);
		unsigned format(_in unsigned offset, _in cstr_t format, _in va_list args);			// не изменяет size, возвращает число записанных символов
	};

	struct padding {
		struct line_separator {
			char_t ch;
			std::pair<unsigned, unsigned> count;
		};
		struct ctor_dtor {
			std::pair<char_t, char_t> ch;
			unsigned count;
		};
		struct line_separator line_separator;
		struct ctor_dtor ctor_dtor;
	};

protected:
	unsigned trace(_in cstr_t function /*= __FUNCTIONW__*/, _in trace::categoty category, _in cstr_t format, _in va_list args);

	void string__create_prefix(_in cstr_t function /*= __FUNCTIONW__*/);
	void string__append_userdata(_in trace::categoty category, _in cstr_t format, _in va_list args);
	//void string__append_n();

	void output() const;
	void output_to__monitor() const;
	void output_to__file() const;

protected:
	constexpr static padding _padding {{L'=', {96, 4}}, {{L'<', L'>'}, 2}};

private:	
	string _string;
	const settings _settings;
};
