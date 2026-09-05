#include <ESPressio_Lua.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <thread>
#include <atomic>
using namespace ESPressio::Lua;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::abort(); } } while (false)
static void good(Result r) { if (!r) { std::fprintf(stderr, "Unexpected error: %s\n", r.message); std::abort(); } }

struct Colour {
    static inline int live = 0;
    int red, green, blue;
    const int channels = 3;
    Colour() : Colour(0,0,0) {}
    Colour(int r, int g, int b) : red(r), green(g), blue(b) { ++live; }
    ~Colour() noexcept { --live; }
    void clear() { red = green = blue = 0; }
    int brightness() const noexcept { return red; }
    void setBrightness(int value) { red = value; }
    int count() const { return 3; }
};
static Type<Colour> colourType(const char* name = "Colour") {
    Type<Colour> type{name};
    type.constructor<>().constructor<int,int,int>()
        .field("red", &Colour::red).field("green", &Colour::green).field("blue", &Colour::blue)
        .field("channels", &Colour::channels).readOnlyField("readonlyRed", &Colour::red)
        .method("clear", &Colour::clear)
        .property("brightness", &Colour::brightness, &Colour::setBrightness)
        .readOnlyProperty("count", &Colour::count);
    return type;
}

static void bindingsAndLifetime() {
    CHECK(Colour::live == 0);
    {
        Colour borrowed{1,2,3};
        {
            Instance vm;
            good(vm.initializationResult());
            auto type = colourType();
            good(vm.registerType(type));
            good(vm.registerType(type));
            CHECK(type.frozen());
            bool frozenRejected = false;
            try { type.field("other", &Colour::red); } catch (const BindingError&) { frozenRejected = true; }
            CHECK(frozenRejected);
            good(vm.registerInstance("native", type, borrowed, Borrowed));
            good(vm.execute(R"(
                local c = Colour(0,128,255)
                assert(c.red == 0 and c.green == 128 and c.blue == 255)
                c.red = 64
                c.brightness = 80
                assert(c.brightness == 80 and c.count == 3)
                c:clear()
                assert(c.red == 0 and c.green == 0 and c.blue == 0)
                assert(not pcall(function() c.count = 4 end))
                assert(not pcall(function() c.channels = 4 end))
                assert(not pcall(function() c.readonlyRed = 4 end))
                assert(not pcall(function() c.unknown = 4 end))
                assert(not pcall(function() return c.unknown end))
                assert(not pcall(function() c.red = '2' end))
                assert(not pcall(function() c.red = 1.25 end))
                assert(not pcall(function() c.red = 9223372036854775807 end))
                assert(not pcall(function() c:clear(1) end))
                assert(not pcall(function() c.clear() end))
                assert(not pcall(function() Colour(1) end))
                assert(not pcall(function() Colour('1',2,3) end))
                native.red = 99
                kept = Colour()
            )"));
            CHECK(borrowed.red == 99);
            good(vm.collectGarbage());
            CHECK(Colour::live == 2); // borrowed and the retained Lua object
        }
        CHECK(Colour::live == 1); // VM closed; borrowed survives
    }
    CHECK(Colour::live == 0);
    // Descriptors may leave scope before the VM: all member closures retain valid backing data.
    Instance vm;
    { auto temporary = colourType(); good(vm.registerType(temporary)); }
    good(vm.execute("retained=Colour(1,2,3); retained:clear(); assert(retained.red==0)"));
}

static void independentViews() {
    Colour a{1,2,3}, b{4,5,6};
    auto type = colourType();
    Type<Colour> view{"Monitor"}; view.readOnlyField("red", &Colour::red);
    Instance first, second;
    good(first.registerType(type)); good(second.registerType(type)); good(first.registerType(view));
    good(first.registerInstance("leds", type, a)); good(second.registerInstance("leds", type, b));
    good(first.registerInstance("monitor", view, a));
    good(first.execute(R"(
        leds.red=90
        privateValue=33
        assert(monitor.red==90)
        assert(not pcall(function() Monitor() end))
        assert(not pcall(function() monitor.red=3 end))
        assert(not pcall(function() leds.clear(monitor) end))
        assert(not pcall(function() leds.clear({}) end))
        assert(not pcall(function() setmetatable(leds,{}) end))
        assert(not pcall(function() setmetatable(_G,{}) end))
    )"));
    good(second.execute("assert(leds.red==4 and privateValue==nil and Monitor==nil)"));
    CHECK(a.red == 90 && b.red == 4);
    auto clash = colourType(); CHECK(!first.registerType(clash));
    Type<Colour> unregistered{"Unregistered"}; CHECK(!first.registerInstance("bad", unregistered, a));
    CHECK(!first.registerConstant("leds", 1));
}

static void modules() {
    Instance vm, second;
    {
        Module module{"app"};
        char label[] = "original";
        module.constant("label", label).constant("MAX", 255).function("twice", [](int v) { return v*2; });
        label[0] = 'X';
        good(vm.registerModule(module)); good(vm.registerModule(module)); good(second.registerModule(module));
        bool rejected = false;
        try { module.constant("late", 1); } catch (const BindingError&) { rejected = true; }
        CHECK(rejected);
    }
    good(vm.execute("assert(app.label=='original' and app.MAX==255 and app.twice(3)==6); assert(not pcall(function() app.MAX=1 end)); assert(not pcall(function() return app.missing end))"));
    good(second.execute("assert(app.twice(4)==8)"));
}

static void functionsAndConversions() {
    Instance vm;
    int calls = 0;
    good(vm.registerFunction("add", [&calls](int a, int b) { ++calls; return a+b; }));
    good(vm.registerFunction("echo", [](const Memory::String<>& s) { return s; }));
    good(vm.registerFunction("negate", [](bool b) { return !b; }));
    good(vm.registerFunction("byte", [](std::uint8_t b) { return b; }));
    good(vm.registerFunction("optional", [](std::optional<int> n) { return n; }));
    good(vm.registerFunction("huge", []() { return std::numeric_limits<std::uint64_t>::max(); }));
    good(vm.registerFunction("cstring", [](const char* value) { return value; }));
    good(vm.registerFunction("floating", [](float value) { return value; }));
    good(vm.registerConstant("MAX", 255));
    good(vm.registerConstant("TEXT", "hello"));
    enum class Mode { Training = 3 };
    good(vm.registerConstant("TRAINING", Mode::Training));
    good(vm.execute(R"(
        assert(add(1,2)==3 and TEXT=='hello' and TRAINING==3)
        assert(echo('a\0b')=='a\0b')
        assert(negate(false)==true and byte(255)==255)
        assert(optional(nil)==nil and optional(42)==42)
        assert(cstring(nil)==nil and cstring('abc')=='abc')
        assert(floating(1.5)==1.5)
        assert(not pcall(function() add(1) end))
        assert(not pcall(function() negate(0) end))
        assert(not pcall(function() byte(-1) end))
        assert(not pcall(function() byte(256) end))
        assert(not pcall(function() huge() end))
        assert(not pcall(function() cstring('a\0b') end))
        assert(not pcall(function() floating(math.huge) end))
        assert(not pcall(function() floating(0/0) end))
        assert(not pcall(function() MAX=2 end))
        assert(not pcall(function() add=2 end))
        assert(MAX==255)
        assert(io==nil and os==nil and package==nil and debug==nil and coroutine==nil)
        assert(load==nil and dofile==nil and loadfile==nil and print==nil and rawset==nil)
        function update(value) answer=value+1 end
    )"));
    CHECK(calls == 1);
    good(vm.call("update", 41));
    int answer = 0; good(vm.readGlobal("answer", answer)); CHECK(answer == 42);
    CHECK(!vm.readGlobal("TEXT", answer)); CHECK(answer == 42);
    CHECK(!vm.call("missing"));
    CHECK(!vm.registerConstant("nilValue", std::optional<int>{}));
    CHECK(!vm.registerConstant("", 2));
    CHECK(!vm.registerConstant(std::string_view("a\0b", 3), 2));
}

struct Throwing {
    static inline int live = 0;
    Throwing(int value) { if (value) throw std::runtime_error("constructor failed"); ++live; }
    ~Throwing() noexcept { --live; }
    void fail(const Memory::String<>&) { throw std::runtime_error("method failed"); }
};
static void errorRecovery() {
    Instance vm;
    Type<Throwing> type{"Throwing"}; type.constructor<int>().method("fail", &Throwing::fail);
    good(vm.registerType(type));
    for (int i=0; i<50; ++i) {
        auto r = vm.execute("Throwing(1)"); CHECK(!r); CHECK(std::strstr(r.message,"constructor failed"));
        good(vm.execute("local t=Throwing(0); local ok,err=pcall(function() t:fail(string.rep('x',1000)) end); assert(not ok and string.find(err,'method failed'))"));
        good(vm.collectGarbage()); CHECK(Throwing::live==0);
    }
    auto syntax = vm.execute("local ="); CHECK(syntax.status == Status::SyntaxError);
    CHECK(!vm.execute("error({})"));
    CHECK(vm.execute("while true do end").status == Status::InstructionLimit);
    good(vm.execute("recovered=1"));
    good(vm.registerFunction("nativeThrow", []() -> int { throw 17; }));
    CHECK(!vm.execute("nativeThrow()"));
    good(vm.execute("recovered=2"));
    good(vm.registerFunction("reenter", [&vm]() { return vm.execute("error('unreachable')").status == Status::Busy; }));
    good(vm.execute("assert(reenter())"));
    std::atomic<bool> entered{false}, release{false};
    good(vm.registerFunction("block", [&]() { entered.store(true); while (!release.load()) std::this_thread::yield(); }));
    std::thread worker([&] { good(vm.execute("block()")); });
    while (!entered.load()) std::this_thread::yield();
    CHECK(vm.execute("x=1").status==Status::Busy);
    release.store(true); worker.join();
}

/// A real System provider checking alignment, exact deallocation, failed growth and provider capture.
struct TrackingProvider final : Memory::IMemoryProvider {
    struct Block { std::size_t size, alignment; };
    std::map<void*,Block> blocks;
    bool fail = false;
    int failAfter = -1;
    void* Allocate(std::size_t bytes, std::size_t alignment, Memory::MemoryPolicy) override {
        if (fail || failAfter == 0) return nullptr;
        if (failAfter > 0) --failAfter;
        void* p = ::operator new(bytes, std::align_val_t(std::max(alignment, alignof(std::max_align_t))));
        CHECK(reinterpret_cast<std::uintptr_t>(p) % alignment == 0);
        blocks.emplace(p,Block{bytes,alignment}); return p;
    }
    void Deallocate(void* p,std::size_t bytes,std::size_t alignment,Memory::MemoryPolicy) noexcept override {
        auto found=blocks.find(p); CHECK(found!=blocks.end());
        CHECK(found->second.size==bytes && found->second.alignment==alignment);
        blocks.erase(found); ::operator delete(p,std::align_val_t(std::max(alignment, alignof(std::max_align_t))));
    }
    bool Supports(Memory::MemoryPolicy) const noexcept override {return true;}
};
struct alignas(128) Aligned { int value=7; };
static void finalizerBudgets() {
    Instance vm;
    CHECK(!vm.execute("closing=setmetatable({}, {__gc=function() while true do end end})"));
    good(vm.execute(R"(
        local meta={}
        local object=setmetatable({},meta)
        meta.__gc=function() while true do end end
        assert(not pcall(function() setmetatable(object,meta) end))
        ordinary=setmetatable({}, {__index=function() return 12 end})
        assert(ordinary.value==12)
    )")); // Adding __gc after initial metatable installation does not mark an object for finalization.
    good(vm.collectGarbage());
}

static void memoryFailures() {
    TrackingProvider provider;
    Configuration config; config.memoryProvider=&provider;
    {
        Instance vm(config); good(vm.initializationResult());
        Type<Aligned> aligned{"Aligned"}; aligned.constructor<>().field("value",&Aligned::value);
        good(vm.registerType(aligned)); good(vm.execute("aligned=Aligned(); assert(aligned.value==7)"));
        provider.fail=true;
        CHECK(vm.execute("allocation=string.rep('x',1000000)").status==Status::MemoryError);
        provider.fail=false;
        good(vm.execute("allocation=nil")); good(vm.collectGarbage());
        CHECK(vm.peakMemoryUsed()<=config.memoryLimitBytes);
    }
    CHECK(provider.blocks.empty());
    config.memoryLimitBytes=1;
    {Instance vm(config); CHECK(vm.initializationResult().status==Status::MemoryError); CHECK(!vm.execute("x=1"));}
    CHECK(provider.blocks.empty());
    config.memoryLimitBytes=64*1024;
    {Instance vm(config);good(vm.initializationResult());CHECK(vm.execute("s=string.rep('x',1000000)").status==Status::MemoryError);good(vm.execute("ok=true"));}
    CHECK(provider.blocks.empty());
    // Walk allocation-failure points in initialization and registration; closing must reclaim every block.
    for (int step=0; step<160; ++step) {
        provider.failAfter=step;
        {Instance vm(config);}
        CHECK(provider.blocks.empty());
    }
    provider.failAfter=-1;
    for (int step=0; step<45; ++step) {
        {
            Instance vm(config); good(vm.initializationResult()); auto type=colourType();
            provider.failAfter=step;
            auto result=vm.registerType(type);
            provider.failAfter=-1;
            if (!result) good(vm.registerType(type));
            good(vm.execute("c=Colour(1,2,3); assert(c.red==1)"));
        }
        CHECK(provider.blocks.empty());
    }
}

int main() {
    bindingsAndLifetime(); CHECK(Colour::live==0);
    independentViews(); CHECK(Colour::live==0);
    modules(); functionsAndConversions(); errorRecovery(); finalizerBudgets(); memoryFailures(); CHECK(Colour::live==0);
    std::puts("All Lua 5.5.1 binding, lifetime, isolation, conversion, execution and allocation tests passed.");
}
