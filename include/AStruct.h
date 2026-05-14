#pragma once
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
#include "pch.h"


class AList {
public:
    static AList autoParse(const std::string& array);

    AList& operator <<(const std::string& list);

    AList();

    std::string toArray();

    size_t size();

    class makego {
    private:
        std::string& ref;
        AList& parentList;
        size_t index;
        std::vector<std::string> govec;
    public:
        makego(std::string& str, AList& parent, size_t idx)
            : ref(str), parentList(parent), index(idx) {
        };
        
        class VectorProxy {
        private:
            std::vector<std::string>& vec;
            AList& parentList;
        public:
            VectorProxy(std::vector<std::string>& v, AList& parent)
                : vec(v), parentList(parent) {
            }

            makego operator[](size_t idx);
            size_t size() const { return vec.size(); }
        };
        
        

        operator std::string() const;

        VectorProxy Go();

        std::string Parse();

        makego& operator=(const std::string& other);

    private:
        AList parseStringToAList(const std::string& str);
    };

    makego operator[](size_t index);
    
    void free();

    template<typename T>
    AList& operator <<(const T& list) {
        std::ostringstream oss;
        oss << list;
        vec.push_back(oss.str());
        return *this;
    }

    AList& operator <<(AList& list);
                          
    const std::string& operator[](size_t index) const;

private:
    std::vector<std::string> vec;
};

class AStruct {
public:
    static bool static_searchtext(const std::string& text, const std::string& target);

    static std::vector<std::string> static_splittext(const std::string& text, const std::string& delimiter);

    static std::string static_getDir();

    void CreateStruct(const std::string& Path, const std::string& title, const std::string& header, const std::string& key, const std::string& datas);
    
    int headerCount(const std::string& title);

    std::vector<std::string> extractBlocks_re(const std::string& titleBlock);

    int getKeyCount(const std::string& title, const std::string& header);

    void loaddata(const std::string& lopath);

    std::string getvalue(const std::string& title, const std::string& header, const std::string& key);

    int CacheMAX = 1024;

    AStruct();

    virtual ~AStruct();

    void addkey(const std::string& title, const std::string& header, const std::string& key, const std::string& value);
    
    int tasks_Max = 1035;

    void CheckOutTask();

    void CreateDebuger();

    static std::vector<std::string> parseArray(const std::string& arrayStr);
    
    static std::string parseKey(const std::string& key);

    std::string getvalue(const std::string& title, const int& indexs, const std::string& key);

	size_t getDataSize();

    std::size_t getFileSize(const std::string& filePath) const;

    void changeValue(const std::string& title, const std::string& header,
        const std::string& key, const std::string& datas);

    void changeValue(const std::string& title, const std::string& header,
        const std::string& key, const std::string& datas,
        const std::string& path, bool autosave);

    void changeValue(const std::string& title, const std::string& header,
        const int& keyindex, const std::string& datas,
        const std::string& path, bool autosave);

    std::string getvalue(const std::string& title, const std::string& header, const int& index);

    void DelHeader(const std::string& title, const std::string& header);
    
    void AppendHeader(const std::string& title, const std::string& header, const std::string& key, const std::string& value );
    
    void AppendTitle(const std::string& title, const std::string& header, const std::string& key, const std::string& value);
    
    void DelKey(const std::string& title, const std::string& header, const std::string& key);

    std::string path;

    std::string structdata;

    void trigger_async_save_two(const std::string& data, const std::string& path);

    void singlesaved(const std::string& data, const std::string& filePath);

    bool inCooking = false;

    std::string getCurrentTimeString();

    void clear();

    AList GetKeysFromHeader(const std::string& title, const std::string& header);

    AList GetHeadersFromTitle(const std::string& title);

    AList GetAllTitle();

    size_t MMAP_THRESHOLD = 256 * 1024;

    void Predefine(const std::string& title, const std::string& header, const std::string& key, const std::string& value);
    
    void Predefine(std::initializer_list<std::string> values);

    bool OpenCache = true;
    /*   
    
    std::vector<std::string> version = { "v1.0-2025Year:9Month:30Day" };
   
    void Deltitle(const std::string& title);

    std::string return_path();*/
protected:
    /*
   
    void Trigger_AES();
    virtual void Autocrypt(); 
    
    
    */
    std::vector<std::string> splitBlocks(const std::string& input);

private:
    void trigger_async_save(const std::string& data, const std::string& path);

    static void parseArrayElements(const std::string& content, std::vector<std::string>& result);   

    void save_worker_loop();

    bool searchtext(const std::string& text, const std::string& target);

    std::vector<std::string> splittext(const std::string& text, const std::string& delimiter);
  
    int cachelong = 0;
     
    bool processNewlinesHighPerf(const std::vector<char>& buffer);

    bool loadBufferedHighPerformance();

    static void trim(std::string& str);

    void cachemanugers(const std::string& title, const std::string& header, const std::string& key, const std::string& value);
    
    std::string actual_header_name;

    std::string process_value(const std::string& title, const std::string& header,
        const std::string& key, std::string value,
        const std::string& index_cache_key);

    std::string getDir();

    std::string readFileUltraFast(const std::wstring& filepath);

    std::mutex queue_mutex_;

    std::queue<std::pair<std::string, std::string>> save_queue_;

    std::condition_variable queue_cv_;

    std::atomic<bool> save_worker_running_{ false };

    std::thread save_worker_thread_;

    std::future<void> save_future;

    std::future<void> save_task;

    std::string FixTitle_t(const std::string& titlebox);

    void summonLog(const std::string& function, const std::string& code, int line);

    std::string removeOptimized(const std::string& str, const std::string& target);

    std::unordered_map<std::string, std::string> cachetwo;

    std::list<std::string> cache_fifo_; // 存 key

    std::unordered_map<std::string, std::list<std::string>::iterator> cache_index_;
    /*
   
    std::vector<std::string> cache;
    bool issaved = false;
    bool ConnectAleacScript = false;
    int index = 0;

    
    std::mutex save_mutex;
    std::string latest_data;   

    

    

    static void parseArrayElements(const std::string& content, std::vector<std::string>& result);

    

    void getHeaderName(const std::string& target_header);

    std::string beta_getvalue(const std::string& title, const std::string& header, const int& index);
    
    

    


   

    

    void removeWhitespace(std::string& str);

    

    */

    
};

static HMODULE hDll = nullptr;
typedef std::string(*CryptFunc)(const std::string&, const std::string&, const std::string&);
static CryptFunc crypt = nullptr;
static CryptFunc decrypt = nullptr;

class AleaCook : public AStruct {
public:
    bool AutoSave = false;

    void Cook();

    void UnCook();

    void Purity();

    explicit AleaCook(AStruct& src, const std::string& input);

    ~AleaCook();

private:

    AStruct* as = nullptr;

    std::string_view formdata;

	bool Dllsave = false;

    std::string aes_path;

    std::string ast_path;

    std::string key;

    std::string iv;

    bool AEStype;

	std::string Path_to_AES();

    std::string Path_to_AST();

    bool testInput(const std::string& input);

    std::string compressTo16Bytes(const std::string& signature);

    std::string extractFirstHashLine(const std::string& content);

    std::string shuffle32WithSeedOptimized(const std::string& seed, const std::string& target);

    bool AES_type();
};
