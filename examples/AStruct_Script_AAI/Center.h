#pragma once
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <stdexcept>
#include <iostream>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable> 
#include <AAI/hpp/AStruct.h>
#include <memory> // 新增用于智能指针
#pragma comment(lib, "AStruct.lib")
#pragma comment(lib, "ws2_32.lib")
namespace AAI {
	void ConnectToCenter();
	std::string Signature(const std::string& name, const std::string& version, const std::string &Description);
	class AString {
		public:
		AString(const char* str) : data(str) {}
		const char* c_str() const { return data; }
		~AString() {
			data = nullptr;
		}
		std::string to_String();
		private:
		const char* data;
		std::string to_utf8(const std::string& str);
	};
	class EventSystem {
	public:
		EventSystem(AStruct* data) : data(data) {}
		;
		bool ButtonClicked(const std::string& buttonName);
		class LineEdit {
		public:
			 void GetText();
			
		};

		~EventSystem() {
			data = nullptr;
		}
	private:
		AStruct* data;
	};

	

	namespace MESBOX {
		class MessageBoxs {
		public:
			static void INFO(const std::string& title, const std::string& msg);
			 std::string GetInput(const std::string& title, const std::string& msg);
			 void SetData(AStruct* d)
			 {
				 {
					 std::lock_guard<std::mutex> lk(m_mtx);
					 data = d;
					 m_hasNewData = true;
				 }
				 m_cv.notify_all();
			 }
		private:		
			AStruct* data = nullptr;
			std::mutex m_mtx;
			std::condition_variable m_cv;
			bool m_hasNewData = false;
		};
	}
	namespace Tools {
		void Tool_SendMessage(SOCKET sock, const std::string& payload);
		class FrameTimer {
		public:
			using Callback = std::function<void()>;

			FrameTimer() : m_running(false) {}
			~FrameTimer() { stop(); }

			// 启动计时器并在独立线程中以 targetFPS 调度 callback
			void start(Callback callback, double targetFPS = 60.0) {
				if (m_running.load(std::memory_order_acquire)) return;

				m_callback = std::move(callback);
				if (targetFPS <= 0.0) targetFPS = 60.0;
				m_frameDuration = std::chrono::duration<double>(1.0 / targetFPS);
				m_running.store(true, std::memory_order_release);

				m_worker = std::thread(&FrameTimer::run, this);
			}

			// 停止计时器并等待线程结束
			void stop() {
				if (!m_running.load(std::memory_order_acquire)) return;

				m_running.store(false, std::memory_order_release);
				m_cv.notify_all();
				if (m_worker.joinable()) {
					m_worker.join();
				}
			}

		private:
			void run() {
				auto nextFrameTime = std::chrono::steady_clock::now();

				while (m_running.load(std::memory_order_acquire)) {
					// 执行回调（异常在此处捕获以避免线程崩溃）
					try {
						if (m_callback) {
							m_callback();
						}
					}
					catch (...) {
						// 忽略回调内部异常，保持计时器运行
					}

					// 计算下一帧时间（将 floating-point duration 转换为 steady_clock::duration）
					nextFrameTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(m_frameDuration);

					// 睡眠直到下一帧时间
					std::this_thread::sleep_until(nextFrameTime);

					// 如果严重落后（例如主线程阻塞），重置时间基准以防止连续追赶
					auto now = std::chrono::steady_clock::now();
					if (now > nextFrameTime + std::chrono::milliseconds(100)) {
						nextFrameTime = now;
					}
				}
			}

		private:
			std::thread m_worker;
			std::atomic<bool> m_running;
			std::mutex m_mutex;
			std::condition_variable m_cv;
			Callback m_callback;
			std::chrono::duration<double> m_frameDuration{ 1.0 / 60.0 };
		};

	}
	namespace Logger {
		void logs(const std::string& msg);
	}


	namespace Layouts {
		void AddonButton(const std::string& Text, int x=0, int y=0, int width= 100, int height=30);
		class LineEdit {
		public:
			struct location;
			class Methods {
			public:
				Methods(LineEdit* parent) : parent(parent) {}
				void SetText(const std::string& text);
				void AppendText(const std::string& text);
				std::string GetText();
				void Clear();
			private:
				LineEdit* parent;
			};
			class Metals {
			public:
				Metals(LineEdit* parent) : parent(parent) {}
				void OnlyRead(bool onlyRead);
				
			private:

				LineEdit* parent;
			};
			void SetData(AStruct* d)
			{
				{
					std::lock_guard<std::mutex> lk(m_mtx);
					data = d;
					m_hasNewData = true;
				}
				m_cv.notify_all();
			}
			/*void Clear();
			std::string GetText();
			void SetText(const std::string& text);
			void SetName(const std::string& name) { Name = name; }
			void AppendText(const std::string& text);*/
			void AddonLineEdit(const std::string& Text, const location& location = { 0,0,200,30 });

			Metals metal{ this };
			Methods method{ this };
		private:
			struct location {
				int x = 0;
				int y = 0;
				int width = 200;
				int height = 30;
			};
			bool onlyRead = false;
			AStruct* data = nullptr;
			AAI::Tools::FrameTimer m_waitTimer; // 保留，不用也不影响
			std::string Name;

			std::mutex m_mtx;
			std::condition_variable m_cv;
			bool m_hasNewData = false;
		};
		class PushButton {
		public:
			struct location;
			class Metals;
			class Methods;
			class Event;

			class Event {
			public:
				Event(PushButton* parent) : parent(parent) {};
				bool Clicked();
				void SetEvent(AStruct* data);
			private:
				PushButton* parent;
			};

			class Metals{
			public:
				Metals(PushButton* parent) : parent(parent) {};
			private:
				PushButton* parent;
			};

			class Methods{
			public:
				Methods(PushButton* parent) : parent(parent) {};
			private:
				PushButton* parent;
			};

			void AddonButton(const std::string& Text, const location& location = { 0,0,100,30 });

			Event Event{ this};
			Metals metal{ this };
			Methods method{ this };
		private:
			struct location {
				int x = 0;
				int y = 0;
				int width = 100;
				int height = 30;
			};
			std::string buttonName;
			AStruct* data;
			
		};


		class TextEdit {
		public:
			struct location;
			class Metals;
			class Methods;
			class Event;
			void AddonTextEdit(const std::string& Text, const location& location = { 0,0,180,50 });

		private:
			 std::string TextEditName;
			struct location {
				int x = 0;
				int y = 0;
				int width = 180;
				int height = 50;
			};
		};
	}
}