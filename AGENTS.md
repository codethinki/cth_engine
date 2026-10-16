# Agent guide

How to write code and documentation in this repository.
Read it before you change a file. Match the existing code.

## Repository

| Path | Content |
|---|---|
| `cvk/` | C++ wrapper of the Vulkan C API: one class per Vulkan object, objects own their handle |
| `jvk/` | coroutine layer on top of `cvk` (not started) |
| `legacy/` | the old `jvk`, `jolly` and `demo` code, reference only, not built |
| `tests/` | loader smoke tests |
| `lib/cth_cmake` | cmake utilities (submodule) |

- Vulkan 1.3 is the baseline. Do not port code that 1.3 makes unnecessary (render passes, framebuffers, the old sync API).
- Use the Vulkan C API through volk. Do not use Vulkan-Hpp.
- Memory comes from VMA.
- `cvk` destroys immediately. It has no destruction queue. Deferred destruction belongs to `jvk`.
- Commits have no `Co-Authored-By` or "Generated with" lines.

## Branches

- `master` is the main branch. The rebuild goes into it when it is finished.
- `v2` is the main branch of the rebuild until then.
- `_v2/jvk_rebuild` is the current work branch. It goes into `v2`.
- Topic branches off the work branch are named `_v2/_jvk_rebuild/<topic>`.
- `dev` and the `_dev/...` branches are old. Do not use them.
- Work locally. Do not open pull requests for single steps.

## Build and test

```bash
cmake --preset gcc_release
cmake --build out/build/gcc_release
ctest --test-dir out/build/gcc_release --output-on-failure
```

- Presets: `gcc_debug`, `gcc_release`, `clang_debug`, `clang_release`, `msvc_debug`, `msvc_release`.
- `cth_ROOT` points at `../cth/out/install/<preset>`. That install must exist.
- Debug builds enable the validation layers. Run the tests in a debug build before you report a change as done.
- Tests that need more queues than the device has skip. Mesa Intel devices get more queues with `ANV_QUEUE_OVERRIDE=gc=2,c=1`.
- After a new cth install, build with `--clean-first`. The precompiled header does not see the changed cth headers.
- Tests that need a device use `test::Context` (`cvk/tests/src/context.hpp`). It fails the test on validation errors, leaked objects included.
- `VCPKG_ROOT` must point at vcpkg.

## Code style

### Formatting

`.clang-format` is the reference. Short form:

- 4 spaces, no tabs, 120 columns.
- Attached braces: `class X {`, `void f() {`, `if(x) {`.
- No space between a keyword and `(`: `if(`, `for(`, `while(`, `switch(`, `if constexpr(`.
- Pointer and reference bind left: `T* ptr`, `T const& ref`.
- `const` comes after the type: `T const&`, `auto const`.
- Nested namespaces inside a file are indented one level (`namespace dev { ... }`).
- No empty line right after `namespace x {` and no empty line right before its closing `}`.

### Naming

| Kind | Style | Example |
|---|---|---|
| class, struct of `cvk` / `jvk` | `PascalCase` | `Semaphore`, `DeviceTable`, `DestructionQueue` |
| nested config and state | `Config`, `State` | `Instance::State` |
| generic library type, trait, concept | `snake_case` | `basic_deletable`, `handle_t`, `destroyable` |
| type alias | `snake_case_t` | `callback_t`, `queue_set_t` |
| member function | `camelCase` | `optDestroy()`, `createInfo()` |
| free function | `snake_case` | `to_queue_properties()`, `default_debug_callback()` |
| static check function | `debug_check`, `debug_check_handle` | |
| function parameter | `camelCase` | `appName`, `requiredExtensions` |
| local variable | `camelCase` | `createResult`, `requiredExtVec` |
| member variable | `_camelCase` | `_handle`, `_debugMessenger` |
| constant | `UPPER_SNAKE_CASE` | `VALIDATION_LAYERS`, `MAX_SECONDARY_CMD_BUFFERS` |
| template parameter | `PascalCase` | `T`, `Rng`, `Context` |
| macro | `UPPER_SNAKE_CASE` with library prefix | `CVK_RESULT_STABLE_THROW` |
| namespace | short lower case | `cvk`, `cvk::constants`, `cvk::dev` |

- A raw Vulkan handle in a variable or parameter has the prefix `vk`: `vkInstance`, `vkSemaphore`.
- A declaration leaves out a parameter name that repeats the type: `void wrap(State const&)`, `destroy(DeviceTable table, VkSemaphore)`, not `VkSemaphore vkSemaphore`. The definition keeps the name.
- Function parameters were `snake_case` in the legacy code. New code uses `camelCase`. Rename them when you port.
- `cvk::dev` holds implementation details that are not part of the API.

### Language

- Use `constexpr`, `consteval` and `decltype(auto)`. The macros `cxpr`, `cval` and `declauto` do not exist anymore.
- Put `[[nodiscard]]` on every function that returns a value the caller must use.
- Put `explicit` on constructors with one parameter and on tag constructors.
- Use `auto const` for locals that do not change.
- Initialize Vulkan structs with designated initializers. Always write `.sType` and `.pNext`.
- Use deducing `this` (`this auto& self`) where a function must work on the derived type.
- Qualify `std::` names. A `.cpp` file can declare `using std::span;` and similar at the top of its namespace.
- No raw `new` and `delete`. Use `std::unique_ptr`, `cth::move_ptr` and `cth::not_null`.
- Prefer compile time checks (concepts, `static_assert`) to run time checks.
- Never erase types into integers or `void*`.

### Includes

Order in a file:

1. the own header (in a `.cpp`)
2. project headers with quotes: `"cvk/base/instance.hpp"`
3. library headers: `<volk.h>`, `<vk_mem_alloc.h>`, `<cth/...>`
4. standard headers: `<vector>`, `<span>`

Separate the groups with one empty line. Forward declare a class when a header only needs its name.

### Files

- Headers go to `incl/<lib>/<area>/<name>.hpp`, sources to `src/<area>/<name>.cpp`. The folders match.
- Config structs that are large get their own `<name>_config.hpp`.
- Every library has a precompiled header in `src/pch/`.
- Every header starts with `#pragma once`.

### Class layout

Classes that own a Vulkan handle follow this order:

```cpp
class Semaphore {
public:
    struct State;

    // constants

    explicit Semaphore(Core const&);                         // base constructor
    Semaphore(Core const&, State const&);                    // constructs and wraps
    explicit Semaphore(Core const&, create_t);               // constructs and creates

    ~Semaphore() { optDestroy(); }

    void wrap(State const& state);
    void create();
    void destroy();
    void optDestroy() { if(created()) destroy(); }
    State release();

    // operations

    static void destroy(DeviceTable table, VkSemaphore);

    // accessors: created(), get(), ...

protected:
    // helpers that derived classes use

private:
    // helpers
    // members

public:
    // copy and move members
    // static debug_check functions
};
```

- The first public section holds the API: types, constants, lifecycle, operations, accessors.
- The last public section holds what every class has and readers rarely need: copy and move members, `debug_check` functions.
- Classes that own a handle are move only. A class that differs (copyable, not movable, custom move) says so in its `@brief`.
- They store the handle in `cvk::unique_handle<Handle, Deleter>` (`cvk/utility/unique_handle.hpp`): a `std::unique_ptr` with a deleter that holds the destroy context and calls the static `destroy`.
  Deleters: `core_deleter<T>` (the core), `instance_deleter<T>` (the `VkInstance`), `handle_deleter<T>` (no context) or a deleter derived from them.
  Device objects use `core_handle<Handle, T>`, short for `unique_handle<Handle, core_deleter<T>>`.
- Handle and context go in together: `_handle.reset(ptr, core())`. A missing context argument does not compile.
- Move constructor and move assignment are `= default`. The move assignment destroys the old handle with the old context.
- A class that owns more than one object declares the objects that depend on its handle first. `= default` assigns in declaration order: `Instance` declares `_debugMessenger` before `_handle`.
- Classes that do not own their handle (`Queue`, `PhysicalDevice`) keep `cth::move_ptr`.

After the class, in the same header:

```cpp
namespace cvk {
struct Semaphore::State {
    cvk::vk_not_null<VkSemaphore> vkSemaphore;
};
}

namespace cvk {
inline void Semaphore::debug_check(Semaphore const& semaphore) {
    CTH_CRITICAL(!semaphore.created(), "semaphore must be created") {}
}
}
```

### Object lifecycle

| Function | Meaning |
|---|---|
| base constructor | stores the configuration, does not create a Vulkan object |
| `wrap(State)` | takes ownership of an existing Vulkan object, calls `optDestroy()` first |
| `create(...)` | creates a new Vulkan object, calls `optDestroy()` first |
| `destroy()` | destroys the Vulkan object now and invalidates, requires `created()` |
| `optDestroy()` | calls `destroy()` if `created()` |
| `release()` | gives the `State` to the caller and invalidates, does not destroy |
| `static destroy(...)` | destroys the raw handles, used by the deleter |
| `invalidate()` | sets the members to the not created state, does not destroy |

- The destructor calls `optDestroy()`.
- `reset` keeps its std meaning (`std::unique_ptr::reset` destroys) and names Vulkan operations (`vkResetFences`). The lifecycle uses `invalidate()`.
- Device objects take `Core const&` in the base constructor. The `core_deleter` of the handle stores it, `core()` returns it.
- A parent (instance, core, device, pool, ...) does not move and is not destroyed while objects depend on it. Code does not have to support it.
- A failed `create()` calls `invalidate()` before it throws.
- `release()` exists so that an owner can use an object it must not destroy (for example swapchain images).

### Errors

- `CTH_CRITICAL(condition, "message") {}`: precondition check, active in debug builds only.
- `CTH_STABLE_ERR(condition, "message") { invalidate(); throw details->exception(); }`: error that can happen at run time.
- `CTH_WARN(condition, "message") {}`: suspicious state that is not an error.
- `CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "message") {}`: throws `cvk::vk_result_exception` with the `VkResult`.
- The condition is the error case: `CTH_CRITICAL(!created(), ...)`, not the expected case.

### Comment markers

- `//TODO` work that is still open
- `//TEMP` code that must change before it is final
- `//IMPLEMENT` a planned change of the design

No space after `//` in markers. Do not put section comments above namespaces.

## Documentation style

Documentation uses doxygen comments and follows ASD-STE100 (Simplified Technical English), adapted to the short style of this code base.

### Form

- Use `/** ... */` blocks with one tag per line.
- Start after the tag with a lower case letter.
- Do not put a period at the end of a line.
- Write short fragments, not full sentences. Start them with a verb in the present tense: `creates the instance`, `destroys and invalidates`.
- Fragments win over STE100: where STE100 asks for a full sentence or an article, write the shorter fragment.
- Use `@ref` for every name of code: `@ref create()`, `@ref Core::created()`.
- Do not document what the signature already tells. A getter without conditions needs no comment.

### Tags

| Tag | Use |
|---|---|
| `@brief` | what the function does, one line |
| `@details` | more information. `calls:` followed by a list of the functions it calls |
| `@param` | name, then what it is for: `@param config to use` |
| `@tparam` | template parameter, same form as `@param` |
| `@return` | what the result is |
| `@pre` | condition that must be true before the call: `@pre @ref created()` |
| `@post` | condition that is true after the call: `@post @ref created()` |
| `@throws` | exception type and the cause: `@throws cvk::vk_result_exception result of vkCreateInstance()` |
| `@note` | side effect the caller must know: `@note calls @ref optDestroy()` |
| `@attention` | danger that is not a precondition: undefined behavior, not thread safe |

- Write preconditions as `@pre`, not as `@attention requires ...`.
- For an exception that cvk throws itself, write the cause after `reason:`: `@throws cth::except::default_exception reason: required extension missing`.

### Math like notation

Use symbols when they are shorter and clearer than words:

- implication: `nullptr -> no messenger`, `@ref memory == nullptr -> memory unknown`
- conditions and ranges: `@pre 0 < workers < MAX_INT`
- special values: `(constants::WHOLE_SIZE => whole buffer)`

### STE100 rules that apply

1. Use one word for one meaning. Do not use synonyms. The terms in the table below are fixed.
2. Write descriptions with 25 words or fewer and instructions with 20 words or fewer.
3. Write one topic in one fragment. Split two actions into two lines.
4. Use the active voice and the present tense.
5. Write instructions to the caller in the imperative: `call @ref create() first`.
6. Do not use phrasal verbs (`set up`, `carry out`). Use one verb (`create`, `do`).
7. Do not use `-ing` forms as nouns or adjectives.
8. Use `it`, `this` and `they` only when the reference is clear.
9. Write the most important condition first, then the consequence.

| Term | Meaning |
|---|---|
| construct | the C++ object comes into existence |
| create | the Vulkan object comes into existence |
| destroy | the Vulkan object stops to exist |
| wrap | take ownership of an existing Vulkan object |
| release | give the ownership to the caller without destroying |
| invalidate | set the members to the not created state |
| handle | the raw Vulkan handle, for example `VkInstance` |
| state | the `State` struct: everything that `wrap` needs |

### Example

```cpp
/**
 * @brief creates the instance
 * @param messengerConfig std::nullopt -> no messenger, debug builds use the default config
 * @pre volk is initialized
 * @post @ref created()
 * @note calls @ref optDestroy()
 * @throws cvk::vk_result_exception result of vkCreateInstance()
 */
void create(std::optional<DebugMessenger::Config> messengerConfig = std::nullopt);
```

### When you port legacy code

- Check every comment against the code. The legacy comments are not always up to date.
- Rewrite comments to this style.
- Report bugs that you find. Do not fix a behavior change silently.

### Repository text

- Do not use em dashes or en dashes in comments, docs, commit messages or markdown.
