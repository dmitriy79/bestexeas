// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2012 The PPCoin developers
// Copyright (c) 2013-2014 The Peershares developers
// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#include "uint256.h"

#ifndef WIN32
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#else
#ifndef _WIN64
typedef int pid_t; /* define for windows compatiblity */
#endif
#endif
#include <map>
#include <vector>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/foreach.hpp>
#include <boost/interprocess/sync/interprocess_recursive_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/lock_options.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <openssl/sha.h>
#include <openssl/ripemd.h>

#include "netbase.h" // for AddTimeData

typedef long long  int64;
typedef unsigned long long  uint64;

static const int64 COIN = 10000;
static const int64 CENT = 100;

#define loop                for (;;)
#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))
#define UBEGIN(a)           ((unsigned char*)&(a))
#define UEND(a)             ((unsigned char*)&((&(a))[1]))
#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))
#define printf              OutputDebugStringF

#ifdef snprintf
#undef snprintf
#endif
#define snprintf my_snprintf

#ifndef PRI64d
#if defined(_MSC_VER) || defined(__MSVCRT__)
#define PRI64d  "I64d"
#define PRI64u  "I64u"
#define PRI64x  "I64x"
#else
#define PRI64d  "lld"
#define PRI64u  "llu"
#define PRI64x  "llx"
#endif
#endif

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define PAIRTYPE(t1, t2)    std::pair<t1, t2>

// Align by increasing pointer, must have extra space at end of buffer
template <size_t nBytes, typename T>
T* alignup(T* p)
{
    union
    {
        T* ptr;
        size_t n;
    } u;
    u.ptr = p;
    u.n = (u.n + (nBytes-1)) & ~(nBytes-1);
    return u.ptr;
}

#ifdef WIN32
#define MSG_NOSIGNAL        0
#define MSG_DONTWAIT        0

#ifndef S_IRUSR
#define S_IRUSR             0400
#define S_IWUSR             0200
#endif
#define unlink              _unlink
#else
#define _vsnprintf(a,b,c,d) vsnprintf(a,b,c,d)
#define strlwr(psz)         to_lower(psz)
#define _strlwr(psz)        to_lower(psz)
#define MAX_PATH            1024
inline void Sleep(int64 n)
{
    /*Boost has a year 2038 problem— if the request sleep time is past epoch+2^31 seconds the sleep returns instantly.
      So we clamp our sleeps here to 10 years and hope that boost is fixed by 2028.*/
    boost::thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(n>315576000000LL?315576000000LL:n));
}
#endif

#ifndef THROW_WITH_STACKTRACE
#define THROW_WITH_STACKTRACE(exception)  \
{                                         \
    LogStackTrace();                      \
    throw (exception);                    \
}
void LogStackTrace();
#endif






extern std::map<std::string, std::string> mapArgs;
extern std::map<std::string, std::vector<std::string> > mapMultiArgs;
extern std::map<std::string, std::string> mapDividendArgs;
extern bool fDebug;
extern bool fPrintToConsole;
extern bool fPrintToDebugger;
extern bool fRequestShutdown;
extern bool fShutdown;
extern bool fDaemon;
extern bool fServer;
extern bool fCommandLine;
extern std::string strMiscWarning;
extern bool fTestNet;
extern bool fNoListen;
extern bool fLogTimestamps;

#ifdef TESTING
extern int64 nTimeShift;
#endif

void RandAddSeed();
void RandAddSeedPerfmon();
int OutputDebugStringF(const char* pszFormat, ...);
int my_snprintf(char* buffer, size_t limit, const char* format, ...);

/* It is not allowed to use va_start with a pass-by-reference argument.
   (C++ standard, 18.7, paragraph 3). Use a dummy argument to work around this, and use a
   macro to keep similar semantics.
*/
std::string real_strprintf(const std::string &format, int dummy, ...);
#define strprintf(format, ...) real_strprintf(format, 0, __VA_ARGS__)

bool error(const char *format, ...);
void LogException(std::exception* pex, const char* pszThread);
void PrintException(std::exception* pex, const char* pszThread);
void PrintExceptionContinue(std::exception* pex, const char* pszThread);
void ParseString(const std::string& str, char c, std::vector<std::string>& v);
std::string FormatMoney(int64 n, bool fPlus=false);
bool ParseMoney(const std::string& str, int64& nRet);
bool ParseMoney(const char* pszIn, int64& nRet);
std::vector<unsigned char> ParseHex(const char* psz);
std::vector<unsigned char> ParseHex(const std::string& str);
bool IsHex(const std::string& str);
std::vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid = NULL);
std::string DecodeBase64(const std::string& str);
std::string EncodeBase64(const unsigned char* pch, size_t len);
std::string EncodeBase64(const std::string& str);
void ParseParameters(int argc, const char*const argv[]);
bool WildcardMatch(const char* psz, const char* mask);
bool WildcardMatch(const std::string& str, const std::string& mask);
int GetFilesize(FILE* file);
boost::filesystem::path GetDefaultDataDir();
boost::filesystem::path GetDefaultDividendDataDir();
boost::filesystem::path GetDefaultNuBitsDataDir();
const boost::filesystem::path &GetDataDir(bool fNetSpecific = true);
const boost::filesystem::path &GetDividendDataDir(bool fNetSpecific = true);
const boost::filesystem::path &GetNuBitsDataDir(bool fNetSpecific = true);
boost::filesystem::path GetConfigFile();
boost::filesystem::path GetDividendConfigFile();
boost::filesystem::path GetPidFile();
void CreatePidFile(const boost::filesystem::path &path, pid_t pid);
void ReadConfigFile(std::map<std::string, std::string>& mapSettingsRet, std::map<std::string, std::vector<std::string> >& mapMultiSettingsRet);
void ReadDividendConfigFile(std::map<std::string, std::string>& mapSettingsRet);
bool GetStartOnSystemStartup();
bool SetStartOnSystemStartup(bool fAutoStart);
void ShrinkDebugFile();
int GetRandInt(int nMax);
uint64 GetRand(uint64 nMax);
uint256 GetRandHash();
int64 GetTime();
void SetMockTime(int64 nMockTimeIn);
int64 GetAdjustedTime();
std::string FormatProtocolVersion(int nVersionVote);
std::string FormatFullVersion();
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments);
void AddTimeData(const CNetAddr& ip, int64 nTime);
void runCommand(std::string strCommand);


std::string BlocksToTime(int64 blocks);
double DurationInYears(int64 blocks);
double AnnualInterestRatePercentage(int64 rate, int64 blocks);
int64 AnnualInterestRatePercentageToRate(double percentage, int64 blocks);









/** Wrapped boost mutex: supports recursive locking, but no waiting  */
typedef boost::interprocess::interprocess_recursive_mutex CCriticalSection;

/** Wrapped boost mutex: supports waiting but not recursive locking */
typedef boost::interprocess::interprocess_mutex CWaitableCriticalSection;

#ifdef DEBUG_LOCKORDER
void EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry = false);
void LeaveCritical();
#else
void static inline EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry = false) {}
void static inline LeaveCritical() {}
#endif

/** Wrapper around boost::interprocess::scoped_lock */
template<typename Mutex>
class CMutexLock
{
private:
    boost::interprocess::scoped_lock<Mutex> lock;
public:

    void Enter(const char* pszName, const char* pszFile, int nLine)
    {
        if (!lock.owns())
        {
            EnterCritical(pszName, pszFile, nLine, (void*)(lock.mutex()));
#ifdef DEBUG_LOCKCONTENTION
            if (!lock.try_lock())
            {
                printf("LOCKCONTENTION: %s\n", pszName);
                printf("Locker: %s:%d\n", pszFile, nLine);
#endif
            lock.lock();
#ifdef DEBUG_LOCKCONTENTION
            }
#endif
        }
    }

    void Leave()
    {
        if (lock.owns())
        {
            lock.unlock();
            LeaveCritical();
        }
    }

    bool TryEnter(const char* pszName, const char* pszFile, int nLine)
    {
        if (!lock.owns())
        {
            EnterCritical(pszName, pszFile, nLine, (void*)(lock.mutex()), true);
            lock.try_lock();
            if (!lock.owns())
                LeaveCritical();
        }
        return lock.owns();
    }

    CMutexLock(Mutex& mutexIn, const char* pszName, const char* pszFile, int nLine, bool fTry = false) : lock(mutexIn, boost::interprocess::defer_lock)
    {
        if (fTry)
            TryEnter(pszName, pszFile, nLine);
        else
            Enter(pszName, pszFile, nLine);
    }

    ~CMutexLock()
    {
        if (lock.owns())
            LeaveCritical();
    }

    operator bool()
    {
        return lock.owns();
    }

    boost::interprocess::scoped_lock<Mutex> &GetLock()
    {
        return lock;
    }
};

typedef CMutexLock<CCriticalSection> CCriticalBlock;

#define LOCK(cs) CCriticalBlock criticalblock(cs, #cs, __FILE__, __LINE__)
#define LOCK2(cs1,cs2) CCriticalBlock criticalblock1(cs1, #cs1, __FILE__, __LINE__),criticalblock2(cs2, #cs2, __FILE__, __LINE__)
#define TRY_LOCK(cs,name) CCriticalBlock name(cs, #cs, __FILE__, __LINE__, true)

#define ENTER_CRITICAL_SECTION(cs) \
    { \
        EnterCritical(#cs, __FILE__, __LINE__, (void*)(&cs)); \
        (cs).lock(); \
    }

#define LEAVE_CRITICAL_SECTION(cs) \
    { \
        (cs).unlock(); \
        LeaveCritical(); \
    }

#ifdef MAC_OSX
// boost::interprocess::interprocess_semaphore seems to spinlock on OSX; prefer polling instead
class CSemaphore
{
private:
    CCriticalSection cs;
    int val;

public:
    CSemaphore(int init) : val(init) {}

    void wait() {
        do {
            {
                LOCK(cs);
                if (val>0) {
                    val--;
                    return;
                }
            }
            Sleep(100);
        } while(1);
    }

    bool try_wait() {
        LOCK(cs);
        if (val>0) {
            val--;
            return true;
        }
        return false;
    }

    void post() {
        LOCK(cs);
        val++;
    }
};
#else
typedef boost::interprocess::interprocess_semaphore CSemaphore;
#endif

inline std::string i64tostr(int64 n)
{
    return strprintf("%"PRI64d, n);
}

inline std::string itostr(int n)
{
    return strprintf("%d", n);
}

inline int64 atoi64(const char* psz)
{
#ifdef _MSC_VER
    return _atoi64(psz);
#else
    return strtoll(psz, NULL, 10);
#endif
}

inline int64 atoi64(const std::string& str)
{
#ifdef _MSC_VER
    return _atoi64(str.c_str());
#else
    return strtoll(str.c_str(), NULL, 10);
#endif
}

inline int atoi(const std::string& str)
{
    return atoi(str.c_str());
}

inline int roundint(double d)
{
    return (int)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64 roundint64(double d)
{
    return (int64)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64 abs64(int64 n)
{
    return (n >= 0 ? n : -n);
}

static char hexmap[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

template<typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces=false)
{
    std::vector<char> rv;

    rv.reserve((itend-itbegin)*3);
    for(T it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if(fSpaces && it != itbegin)
            rv.push_back(' ');
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
    }

    return std::string(rv.begin(), rv.end());
}

template<typename T>
inline std::string HexStr(const T& vch, bool fSpaces=false)
{
    return HexStr(vch.begin(), vch.end(), fSpaces);
}

template<typename T>
void PrintHex(const T pbegin, const T pend, const char* pszFormat="%s", bool fSpaces=true)
{
    printf(pszFormat, HexStr(pbegin, pend, fSpaces).c_str());
}

inline void PrintHex(const std::vector<unsigned char>& vch, const char* pszFormat="%s", bool fSpaces=true)
{
    printf(pszFormat, HexStr(vch, fSpaces).c_str());
}

inline int64 GetPerformanceCounter()
{
    int64 nCounter = 0;
#ifdef WIN32
    QueryPerformanceCounter((LARGE_INTEGER*)&nCounter);
#else
    timeval t;
    gettimeofday(&t, NULL);
    nCounter = t.tv_sec * 1000000 + t.tv_usec;
#endif
    return nCounter;
}

inline int64 GetTimeMillis()
{
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_milliseconds();
}

inline std::string DateTimeStrFormat(const char* pszFormat, int64 nTime)
{
    time_t n = nTime;
    struct tm* ptmTime = gmtime(&n);
    char pszTime[200];
    strftime(pszTime, sizeof(pszTime), pszFormat, ptmTime);
    return pszTime;
}

static const std::string strTimestampFormat = "%Y-%m-%d %H:%M:%S UTC";
inline std::string DateTimeStrFormat(int64 nTime)
{
    return DateTimeStrFormat(strTimestampFormat.c_str(), nTime);
}

template<typename T>
void skipspaces(T& it)
{
    while (isspace(*it))
        ++it;
}

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArg(const std::string& strArg, const std::string& strDefault);
std::string GetDividendArg(const std::string& strArg, const std::string& strDefault);

/**
 * Return integer argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64 GetArg(const std::string& strArg, int64 nDefault);
int64 GetDividendArg(const std::string& strArg, int64 nDefault);

/**
 * Return boolean argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool GetBoolArg(const std::string& strArg, bool fDefault=false);
bool GetDividendBoolArg(const std::string& strArg, bool fDefault=false);

/**
 * Set an argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param strValue Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetArg(const std::string& strArg, const std::string& strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param fValue Value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetBoolArg(const std::string& strArg, bool fValue);









// Randomize the stack to help protect against buffer overrun exploits
#define IMPLEMENT_RANDOMIZE_STACK(ThreadFn)     \
    {                                           \
        static char nLoops;                     \
        if (nLoops <= 0)                        \
            nLoops = GetRand(20) + 1;           \
        if (nLoops-- > 1)                       \
        {                                       \
            ThreadFn;                           \
            return;                             \
        }                                       \
    }


template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T1, typename T2, typename T3>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end,
                    const T3 p3begin, const T3 p3end)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Update(&ctx, (p3begin == p3end ? pblank : (unsigned char*)&p3begin[0]), (p3end - p3begin) * sizeof(p3begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T>
uint256 SerializeHash(const T& obj, int nType=SER_GETHASH, int nVersion=PROTOCOL_VERSION)
{
    // Most of the time is spent allocating and deallocating CDataStream's
    // buffer.  If this ever needs to be optimized further, make a CStaticStream
    // class with its buffer on the stack.
    CDataStream ss(nType, nVersion);
    ss.reserve(10000);
    ss << obj;
    return Hash(ss.begin(), ss.end());
}

inline uint160 Hash160(const std::vector<unsigned char>& vch)
{
    uint256 hash1;
    SHA256(&vch[0], vch.size(), (unsigned char*)&hash1);
    uint160 hash2;
    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}


/** Median filter over a stream of values. 
 * Returns the median of the last N numbers
 */
template <typename T> class CMedianFilter
{
private:
    std::vector<T> vValues;
    std::vector<T> vSorted;
    unsigned int nSize;
public:
    CMedianFilter(unsigned int size, T initial_value):
        nSize(size)
    {
        vValues.reserve(size);
        vValues.push_back(initial_value);
        vSorted = vValues;
    }
    
    void input(T value)
    {
        if(vValues.size() == nSize)
        {
            vValues.erase(vValues.begin());
        }
        vValues.push_back(value);

        vSorted.resize(vValues.size());
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    T median() const
    {
        int size = vSorted.size();
        assert(size>0);
        if(size & 1) // Odd number of elements
        {
            return vSorted[size/2];
        }
        else // Even number of elements
        {
            return (vSorted[size/2-1] + vSorted[size/2]) / 2;
        }
    }

    int size() const
    {
        return vValues.size();
    }

    std::vector<T> sorted () const
    {
        return vSorted;
    }
};










// Note: It turns out we might have been able to use boost::thread
// by using TerminateThread(boost::thread.native_handle(), 0);
#ifdef WIN32
typedef HANDLE bitcoin_pthread_t;

inline bitcoin_pthread_t CreateThread(void(*pfn)(void*), void* parg, bool fWantHandle=false)
{
    DWORD nUnused = 0;
    HANDLE hthread =
        CreateThread(
            NULL,                        // default security
            0,                           // inherit stack size from parent
            (LPTHREAD_START_ROUTINE)pfn, // function pointer
            parg,                        // argument
            0,                           // creation option, start immediately
            &nUnused);                   // thread identifier
    if (hthread == NULL)
    {
        printf("Error: CreateThread() returned %d\n", GetLastError());
        return (bitcoin_pthread_t)0;
    }
    if (!fWantHandle)
    {
        CloseHandle(hthread);
        return (bitcoin_pthread_t)-1;
    }
    return hthread;
}

inline void SetThreadPriority(int nPriority)
{
    SetThreadPriority(GetCurrentThread(), nPriority);
}
#else
inline pthread_t CreateThread(void(*pfn)(void*), void* parg, bool fWantHandle=false)
{
    pthread_t hthread = 0;
    int ret = pthread_create(&hthread, NULL, (void*(*)(void*))pfn, parg);
    if (ret != 0)
    {
        printf("Error: pthread_create() returned %d\n", ret);
        return (pthread_t)0;
    }
    if (!fWantHandle)
    {
        pthread_detach(hthread);
        return (pthread_t)-1;
    }
    return hthread;
}

#define THREAD_PRIORITY_LOWEST          PRIO_MAX
#define THREAD_PRIORITY_BELOW_NORMAL    2
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_ABOVE_NORMAL    0

inline void SetThreadPriority(int nPriority)
{
    // It's unclear if it's even possible to change thread priorities on Linux,
    // but we really and truly need it for the generation threads.
#ifdef PRIO_THREAD
    setpriority(PRIO_THREAD, 0, nPriority);
#else
    setpriority(PRIO_PROCESS, 0, nPriority);
#endif
}

inline void ExitThread(size_t nExitCode)
{
    pthread_exit((void*)nExitCode);
}
#endif





inline uint32_t ByteReverse(uint32_t value)
{
    value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
    return (value<<16) | (value>>16);
}


/**
 * Asset IDs can be random numbers or encode capital Latin characters typically the asset symbol.
 *
 * The 2 HO bits encode the id header and it can take 3 values: letters only, letters and numbers and raw value.
 *
 * The raw id is a number that has the header bits set to 00.
 *
 * The alphanumeric IDs are encoded with 5 or 6 bits per character depending if it encodes numbers as well.
 *
 * The header 11 is reserved and the ID 0xFFFFFFFF is the invalid constant
 */

#define ASSET_SYMBOL_ALPHA_MAX          (6)
#define ASSET_SYMBOL_ALPHANUM_MAX       (5)
#define ASCII_ALPHA_START               (0x41)  // 'A'
#define ASCII_ALPHA_END                 (0x5A)  // 'Z'
#define ASCII_NUM_START                 (0x30)  // '0'
#define ASCII_NUM_END                   (0x39)  // '9'
#define ASCII_ALPHA_ENCODING_BASE       (0x40)  // so that A-Z will be in the rage 0x01- 0x1A
#define ASCII_NUM_ENCODING_BASE         (0x10)  // so that 0-1 will be in the rage 0x20- 0x29
#define ASCII_ENCODED_ALPHA_START       (ASCII_ALPHA_START - ASCII_ALPHA_ENCODING_BASE)
#define ASCII_ENCODED_ALPHA_END         (ASCII_ALPHA_END - ASCII_ALPHA_ENCODING_BASE)
#define ASCII_ENCODED_NUM_START         (ASCII_NUM_START - ASCII_NUM_ENCODING_BASE)
#define ASCII_ENCODED_NUM_END           (ASCII_NUM_END - ASCII_NUM_ENCODING_BASE)
#define ASSET_ID_ALPHA_SHIFT            (5)     // 5 bits per character
#define ASSET_ID_ALPHA_CHAR_MASK        (0x1f)
#define ASSET_ID_ALPHANUM_SHIFT         (6)     // 6 bits per character
#define ASSET_ID_ALPHANUM_CHAR_MASK     (0x3f)
#define ASSET_ID_RAW_CHARS              (12)    // In ID0000000123 format
#define ASSET_ID_RAW_MAX_VALUE          (1073741823) // 0x3FFFFFFF
#define ASSET_ID_HEADER_SHIFT           (30)    // Where the header is located
#define ASSET_ID_HEADER_MASK            (0xC0000000)
#define ASSET_ID_HEADER_RAW             (0x0)   // 00.. - the id has no encoding
#define ASSET_ID_HEADER_ALPHA           (0x1)   // 01.. - id contains only letters + 5 undefined symbols
#define ASSET_ID_HEADER_ALPHANUM        (0x2)   // 10.. - contains letters, numbers and many more symbols
#define ASSET_ID_HEADER_RESERVED        (0x3)   // 11.. - reserved
#define ASSET_ID_INVALID                (0xFFFFFFFF)
#define ASSET_ID_INVALID_ZERO           (0)
#define ASSET_ID_INVALID_EMPTY_ALPHA    (ASSET_ID_HEADER_ALPHA << ASSET_ID_HEADER_SHIFT)
#define ASSET_ID_INVALID_EMPTY_ALPHANUM (ASSET_ID_HEADER_ALPHANUM << ASSET_ID_HEADER_SHIFT)
#define ASSET_ID_INVALID_STR            ("INVALID_ID")


inline static uint8_t GetAssetIdHeader(uint32_t nId)
{
    return (nId & ASSET_ID_HEADER_MASK) >> ASSET_ID_HEADER_SHIFT;
}

uint32_t EncodeAssetId(std::string symbol);

inline uint32_t EncodeAssetId(uint32_t nRawId)
{
    if (GetAssetIdHeader(nRawId) == ASSET_ID_HEADER_RAW && nRawId != ASSET_ID_INVALID_ZERO)
        return nRawId;
    else
        return ASSET_ID_INVALID;
}

std::string AssetIdToStr(uint32_t nId);

bool IsValidAssetId(uint32_t nId);

/*
 * Pre-calculated table for exponential series that follow the pattern:
 *
 * 0, 1, 2, ... , 8, 9, 10, 20, ... , 80, 90, 100, 200, ... , 8E17, 9E17, 1E18, 2E18, ... , 8E18, 9E18
 *
 * The maximum value 9E18 is lower than the highest signed 64 bit integer (9223372036854775807L).
 * The parameter range is [0, 171].
 *
 * The function ExponentialParameter(int64 value) returns the highest parameter where:
 *
 *     pnExponentialSeries[ExponentialParameter(value)] <= value
 *
 * The following python lambda produces the teble values:
 * lambda(i):(i%9) * 10**(i/9) if i == 0 or i%9 != 0 else 9*10**(i/9 - 1)
 */
#define EXP_SERIES_MAX_PARAM (171)

unsigned char ExponentialParameter(int64 value);
unsigned char ConvertExpParameter(unsigned char nExpParam, unsigned char nFromUnitExponent, unsigned char nToUnitExponent);

static const int64 pnExponentialSeries[] = {
    0L, // 0
    1L, // 1
    2L, // 2
    3L, // 3
    4L, // 4
    5L, // 5
    6L, // 6
    7L, // 7
    8L, // 8
    9L, // 9
    10L, // 10
    20L, // 11
    30L, // 12
    40L, // 13
    50L, // 14
    60L, // 15
    70L, // 16
    80L, // 17
    90L, // 18
    100L, // 19
    200L, // 20
    300L, // 21
    400L, // 22
    500L, // 23
    600L, // 24
    700L, // 25
    800L, // 26
    900L, // 27
    1000L, // 28
    2000L, // 29
    3000L, // 30
    4000L, // 31
    5000L, // 32
    6000L, // 33
    7000L, // 34
    8000L, // 35
    9000L, // 36
    10000L, // 37
    20000L, // 38
    30000L, // 39
    40000L, // 40
    50000L, // 41
    60000L, // 42
    70000L, // 43
    80000L, // 44
    90000L, // 45
    100000L, // 46
    200000L, // 47
    300000L, // 48
    400000L, // 49
    500000L, // 50
    600000L, // 51
    700000L, // 52
    800000L, // 53
    900000L, // 54
    1000000L, // 55
    2000000L, // 56
    3000000L, // 57
    4000000L, // 58
    5000000L, // 59
    6000000L, // 60
    7000000L, // 61
    8000000L, // 62
    9000000L, // 63
    10000000L, // 64
    20000000L, // 65
    30000000L, // 66
    40000000L, // 67
    50000000L, // 68
    60000000L, // 69
    70000000L, // 70
    80000000L, // 71
    90000000L, // 72
    100000000L, // 73
    200000000L, // 74
    300000000L, // 75
    400000000L, // 76
    500000000L, // 77
    600000000L, // 78
    700000000L, // 79
    800000000L, // 80
    900000000L, // 81
    1000000000L, // 82
    2000000000L, // 83
    3000000000L, // 84
    4000000000L, // 85
    5000000000L, // 86
    6000000000L, // 87
    7000000000L, // 88
    8000000000L, // 89
    9000000000L, // 90
    10000000000L, // 91
    20000000000L, // 92
    30000000000L, // 93
    40000000000L, // 94
    50000000000L, // 95
    60000000000L, // 96
    70000000000L, // 97
    80000000000L, // 98
    90000000000L, // 99
    100000000000L, // 100
    200000000000L, // 101
    300000000000L, // 102
    400000000000L, // 103
    500000000000L, // 104
    600000000000L, // 105
    700000000000L, // 106
    800000000000L, // 107
    900000000000L, // 108
    1000000000000L, // 109
    2000000000000L, // 110
    3000000000000L, // 111
    4000000000000L, // 112
    5000000000000L, // 113
    6000000000000L, // 114
    7000000000000L, // 115
    8000000000000L, // 116
    9000000000000L, // 117
    10000000000000L, // 118
    20000000000000L, // 119
    30000000000000L, // 120
    40000000000000L, // 121
    50000000000000L, // 122
    60000000000000L, // 123
    70000000000000L, // 124
    80000000000000L, // 125
    90000000000000L, // 126
    100000000000000L, // 127
    200000000000000L, // 128
    300000000000000L, // 129
    400000000000000L, // 130
    500000000000000L, // 131
    600000000000000L, // 132
    700000000000000L, // 133
    800000000000000L, // 134
    900000000000000L, // 135
    1000000000000000L, // 136
    2000000000000000L, // 137
    3000000000000000L, // 138
    4000000000000000L, // 139
    5000000000000000L, // 140
    6000000000000000L, // 141
    7000000000000000L, // 142
    8000000000000000L, // 143
    9000000000000000L, // 144
    10000000000000000L, // 145
    20000000000000000L, // 146
    30000000000000000L, // 147
    40000000000000000L, // 148
    50000000000000000L, // 149
    60000000000000000L, // 150
    70000000000000000L, // 151
    80000000000000000L, // 152
    90000000000000000L, // 153
    100000000000000000L, // 154
    200000000000000000L, // 155
    300000000000000000L, // 156
    400000000000000000L, // 157
    500000000000000000L, // 158
    600000000000000000L, // 159
    700000000000000000L, // 160
    800000000000000000L, // 161
    900000000000000000L, // 162
    1000000000000000000L, // 163
    2000000000000000000L, // 164
    3000000000000000000L, // 165
    4000000000000000000L, // 166
    5000000000000000000L, // 167
    6000000000000000000L, // 168
    7000000000000000000L, // 169
    8000000000000000000L, // 170
    9000000000000000000L  // 171
};

#endif
