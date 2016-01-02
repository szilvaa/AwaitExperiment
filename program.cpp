#include <future>
#include <iostream>
#include <string>
#include <deque>

typedef void (*CallbackPtr)(void* userData);

#define TRACE0() std::cout << __FUNCTION__ << "()\n"
#define TRACE1(x) std::cout << __FUNCTION__ << "(" << x << ")\n"

#define TRACEINST0() std::cout << __FUNCTION__ << "#" << m_count << "()\n"
#define TRACEINST1(x) std::cout << __FUNCTION__ << "#" << m_count  << "(" << x << ")\n"

struct CommandState
{
    bool m_complete;
};
class CommandAwaitable
{
    static int count;
    int m_count;
    std::unique_ptr<CommandState> m_state;
public:
    CommandAwaitable(bool complete)
        :m_count(count++)
        ,m_state(std::make_unique<CommandState>())
    {
        m_state->m_complete = complete;
        TRACEINST0();
    }
    CommandAwaitable(CommandState* state) noexcept
        :m_count(count++)
        , m_state(std::move(state))
    {
        TRACEINST0();
    }
    CommandAwaitable(CommandAwaitable&& other)
        :m_count(count++)
        ,m_state(std::move(other.m_state))
    {
        TRACEINST0();
    }
    bool isComplete() const noexcept
    {
        TRACEINST0();
        return m_state->m_complete;
    }
    CommandAwaitable(const CommandAwaitable&) = delete;
    CommandAwaitable& operator = (const CommandAwaitable&) = delete;
};

class CommandResumable
{
    CommandState* m_state = nullptr;
public:
    CommandAwaitable get_return_object()
    {
        TRACE0();
        m_state = new CommandState{ false };
        return CommandAwaitable(m_state);
    }
    bool initial_suspend() const
    {
        return false;
    }
    bool final_suspend() const
    {
        return false;
    }
    void return_void() 
    {
        TRACE0();
        m_state->m_complete = true;
    }
};
int CommandAwaitable::count = 0;
struct HandlerState
{
    bool complete;
    bool result;
};
class HandlerAwaitable
{
    static int count;
    int m_count;
    std::unique_ptr<HandlerState> m_state;
public:
    HandlerAwaitable(bool v) noexcept
        : m_state(std::make_unique<HandlerState>())
        , m_count(count++)
    {
        m_state->complete = true;
        m_state->result = v;
        TRACEINST0();
    }
    HandlerAwaitable(HandlerState* state) noexcept
        : m_state(state)
        , m_count(count++)
    {
        TRACEINST0();
    }
    HandlerAwaitable(HandlerAwaitable&& other)
        :m_state(std::move(other.m_state))
        ,m_count(count++)
    {

    }
    ~HandlerAwaitable() noexcept
    {
        TRACEINST0();
    }
    bool isReady() const noexcept
    {
        TRACEINST0();
        return m_state->complete;
    }
    bool value() const noexcept
    {
        TRACEINST0();
        return m_state->result;
    }
    HandlerState* state() { return m_state.get(); }
};
int HandlerAwaitable::count = 0;
class HandlerResumable
{
    HandlerState* m_state = nullptr;
public:
    HandlerResumable()
    {
        TRACE0();
    }
    ~HandlerResumable()
    {
        TRACE0();
    }
    HandlerAwaitable get_return_object()
    {
        TRACE0();
        m_state = new HandlerState{ false, false };
        return HandlerAwaitable(m_state);
    }
    bool initial_suspend() const
    {
        TRACE0();
        return false;
    }
    bool final_suspend() const
    {
        TRACE0();
        return false;
    }
    void return_value(bool v)
    {
        TRACE0();
        m_state->complete = true;
        m_state->result = v;
    }
    bool isReady() const { return m_state->complete; }
    bool value() const { return m_state->result; }
};


struct Callback
{
    CallbackPtr userFunction;
    void* userData;
};
struct CmdItem
{
    std::deque<Callback> m_callstack;
};
std::string g_token;
class CmdStack
{
    std::vector<std::unique_ptr<CmdItem>> cmdStack;
    CmdItem* cur = nullptr;
    bool resuming = false;
    bool modified = false;
public:
    void push()
    {
        std::cout << "Push\n";
        cmdStack.push_back(std::make_unique<CmdItem>());
        modified = true;
    }
    void pop()
    {
        cmdStack.pop_back();
        modified = true;
        std::cout << "Pop\n";
    }
    bool tryGetToken()
    {
        modified = false;
        for (auto i =rbegin(cmdStack); i != rend(cmdStack); ++i)
        {
            cur = i->get();
            while (!cur->m_callstack.empty())
            {
                auto callback = cur->m_callstack.front().userFunction;
                auto userData = cur->m_callstack.front().userData;
                cur->m_callstack.pop_front();
                resuming = true;
                callback(userData);
                resuming = false;
                if (!g_token.empty() || modified)
                {
                    cur = nullptr;
                    return !modified;
                }
            }
            cur = nullptr;
        }
        return false;
    }
    void setCallback(CallbackPtr ptr, void* userData)
    {
        
        auto c = cur;
        if (c == nullptr)
            c = cmdStack.back().get();
        if (resuming)
            c->m_callstack.push_front({ ptr, userData });
        else
            c->m_callstack.push_back({ ptr, userData });
    }
};

CmdStack cmdStack;

template <>
struct std::experimental::coroutine_traits<CommandAwaitable>
{
    using promise_type = CommandResumable;
};
template <>
struct std::experimental::coroutine_traits<HandlerAwaitable,  std::string>
{
    using promise_type = HandlerResumable;
};
auto operator await(const CommandAwaitable& v) {

    class CommandAwaiter {
        static void callback(void* userData)
        {
            TRACE0();
            std::experimental::coroutine_handle<>::from_address(userData)();
        }
        const CommandAwaitable& cmdRes;
    public:
        explicit CommandAwaiter(const CommandAwaitable& v):cmdRes(v)
        {
            TRACE0();
        }
        bool await_ready() const 
        { 
            TRACE0();
            return cmdRes.isComplete(); 
        }
        bool await_suspend(std::experimental::coroutine_handle<> resume_cb) {
            TRACE0();
            cmdStack.setCallback(&callback, resume_cb.to_address());
            return true;
        }
        void await_resume() 
        {
            TRACE0();
        }
        ~CommandAwaiter()
        {
            TRACE0();
        }
    };

    return CommandAwaiter(v);

}

auto operator await(const HandlerAwaitable& v) {

    class HandlerAwaiter {
        static void callback(void* userData)
        {
            TRACE0();
            std::experimental::coroutine_handle<>::from_address(userData)();
        }
        const HandlerAwaitable& m_result;
    public:
        explicit HandlerAwaiter(const HandlerAwaitable& v)
            :m_result(v)
        {
            TRACE0();
        }
        bool await_ready() const 
        { 
            TRACE0();
            return m_result.isReady(); 
        }
        bool await_suspend(std::experimental::coroutine_handle<> resume_cb) {
            TRACE0();
            cmdStack.setCallback(&callback, resume_cb.to_address());
            return true;
        }
        void await_resume() 
        {
            TRACE0();
        }
        ~HandlerAwaiter()
        {
            TRACE0();
        }
    };

    return HandlerAwaiter(v);

}
void runloop(std::function<
    HandlerAwaitable
    (const std::string token)> handler)
{
    while (true)
    {
        TRACE1("begin");
        if (g_token.empty())
        {
            if (!cmdStack.tryGetToken())
                std::getline(std::cin, g_token);
        }
        auto tmp(g_token);
        g_token.erase();
        auto&& res = handler(tmp);
        if (res.isReady() && res.value())
        {
            TRACE1("end");
            return;
        }
    }
}
CommandAwaitable commandAsync(const std::string& t)
{
    g_token = t;
    TRACE1(t);
    return CommandAwaitable(false);
}

CommandAwaitable line()
{
    std::cout << "Begin LINE command\n";
    runloop([](const std::string token) {std::cout << "Got first " << token << "\n"; return true;});
    runloop([](const std::string token) {std::cout << "Got second " << token << "\n"; return true;});
    std::cout << "End LINE command\n";
    return CommandAwaitable(true);
}
CommandAwaitable testWorker() 
{
    std::cout << "Begin testWorker ...\n";
    await commandAsync("LINE");
    std::cout << "Before 0,0 ...\n";
    await commandAsync("0,0");
    std::cout << "Before 1,1 ...\n";
    await commandAsync("1,1");
    std::cout << "End testWorker ...\n";
}
CommandAwaitable test()
{
    std::cout << "Begin test\n";
    await testWorker();
    std::cout << "End test\n";
}
typedef CommandAwaitable(*CmdCallbackPtr)();

struct Cmd
{
    const char* name;
    CmdCallbackPtr ptr;
};

HandlerAwaitable
dispatch(const std::string token)
{
    std::cout << "Begin dispatch " << token << "\n";
    static Cmd cmds[]{
        {"LINE", line},
        {"TEST", test}
    };
    for (auto&& cmd:cmds )
    {
        if (_stricmp(cmd.name, token.c_str()) == 0)
        {
            cmdStack.push();
            await cmd.ptr();
            cmdStack.pop();
            break;
        }
    }
    std::cout << "End dispatch " << token << "\n";
    if (_stricmp("QUIT", token.c_str()) == 0)
        return true ;
    return false;
}


int main() 
{
    runloop(dispatch);
}


