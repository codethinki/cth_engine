# Agent guide

How to write code and documentation in this repository.
Read it before you change a file. Match the existing code.

## Repository

| Path            | Content                                                                                       |
|-----------------|-----------------------------------------------------------------------------------------------|
| `cvk/`          | C++ wrapper of the Vulkan C API: one class per Vulkan object, objects own their handle        |
| `cgfw/`         | C++ wrapper of RGFW: windows of the platform (Win32, Wayland) and their Vulkan surface        |
| `jvk/`          | coroutine layer on top of `cvk` (in progress): threads, queues, stage graph, object lifetimes |
| `tests/`        | loader smoke tests                                                                            |
| `lib/cth`       | cth (submodule), `lib/cmake/cth.cmake` adds it with only the components the engine needs      |
| `lib/cth_cmake` | cmake utilities (submodule)                                                                   |
| `lib/rgfw`      | RGFW windowing library, our fork (submodule) on branch `cth_engine`, fixes are commits there  |
| `lib/cmake`     | one cmake file per library that adds it: `<libname>.cmake`                                    |

- Vulkan 1.3 is the baseline. Do not port code that 1.3 makes unnecessary (render passes, framebuffers, the old sync
  API).
- Features beyond 1.3 come from required extensions, not from a higher version.
- Device features are the `bool` fields of `DeviceFeatures`. A true bit of an extension struct enables its extension.
  New extension struct: entry in `dev::feature_structs_t`, `dev::feature_struct` specialization, fields in
  `DeviceFeatures` in the same order.
- Use the Vulkan C API through volk. Do not use Vulkan-Hpp.
- Memory comes from VMA.
- `cvk` keeps only the state it needs to work: no destruction queue, no layout tracking, no thread sync.
    - other state is a parameter, with a default where the use fixes it: the image layout
    - `jvk` tracks it, the validation layers do the checks that need it
- Hash maps: `cth::dt::flat_hash_map`, `cth::dt::flat_hash_set`. No `std::unordered_map`.
- Commits have no `Co-Authored-By` or "Generated with" lines.

## Branches

- `master` is the main branch.
- Work happens on a work branch off `master`. Its pull request into `master` runs the CI on every push.
- `v2`, `_v2/...`, `dev` and `_dev/...` are old. Do not use them.
- Work locally. Do not open pull requests for single steps.
- CI runs on pushes and pull requests to `master`.

## Build and test

```bash
cmake --preset gcc_release
cmake --build out/build/gcc_release
ctest --test-dir out/build/gcc_release --output-on-failure
```

- Presets: `gcc_debug`, `gcc_release`, `clang_debug`, `clang_release`, `msvc_debug`, `msvc_release`.
- cth comes from the submodule `lib/cth`: clone with `--recursive` or run `git submodule update --init`.
- Debug builds enable the validation layers with synchronization validation (`Instance::VALIDATION_LAYER_SETTINGS`). Run
  the tests in a debug build before you report a change as done.
- Tests that need more queues than the device has skip. Mesa Intel devices get more queues with
  `ANV_QUEUE_OVERRIDE=gc=2,c=1`.
- Tests that need a device use `test::Context` (`cvk/tests/src/context.hpp`). It fails the test on validation errors,
  leaked objects included.
- `VCPKG_ROOT` must point at vcpkg.
- `doxygen` in the repository root checks the docs (`Doxyfile`, output in `out/doxygen`). A change adds no doxygen
  warning.

## Code style

### Formatting

The CLion code style of the project (`.idea/codeStyles/Project.xml`) is the reference. Short form:

- 4 spaces, no tabs, 100 columns.
- Attached braces: `class X {`, `void f() {`, `if(x) {`.
- The statement of an `if`, `for` or `while` without braces goes on its own line.
- `catch` starts a new line. A simple `try` or `catch` block stays on one line: `try { x(); }`.
- A wrapped binary expression starts the next line with the operator: `&& next`.
- Break a long condition by hand before `&&` and `||`. The formatter keeps the break, its own wrap can split
  a member access.
- No space between a keyword and `(`: `if(`, `for(`, `while(`, `switch(`, `if constexpr(`.
- Pointer and reference bind left: `T* ptr`, `T const& ref`.
- `const` comes after the type: `T const&`, `auto const`.
- Nested namespaces inside a file are indented one level (`namespace dev { ... }`).
- No empty line right after `namespace x {` and no empty line right before its closing `}`.

### Naming

| Kind                                                    | Style                                  | Example                                             |
|---------------------------------------------------------|----------------------------------------|-----------------------------------------------------|
| type of the Vulkan abstraction: object, data struct     | `PascalCase`                           | `Semaphore`, `Stasis`, `SubmitInfo`, `ImageAccess`  |
| nested config and state                                 | `Config`, `State`                      | `Instance::State`                                   |
| general type, like std: handle, deleter, trait, concept | `snake_case`                           | `unique_handle`, `handle_t`, `device_destroyable`   |
| type alias that renames a type                          | `snake_case_t`                         | `callback_t`, `queue_set_t`                         |
| member function                                         | `camelCase`                            | `optDestroy()`, `createInfo()`                      |
| named constructor (static factory)                      | `PascalCase`                           | `CubeMap()`, `DepthBuffer()`, `LayoutTransition()`  |
| free function                                           | `snake_case`                           | `to_queue_properties()`, `default_debug_callback()` |
| static check function                                   | `debug_check`, `debug_check_handle`    |                                                     |
| function parameter                                      | `camelCase`                            | `appName`, `requiredExtensions`                     |
| local variable                                          | `camelCase`                            | `createResult`, `requiredExtVec`                    |
| member variable                                         | `_camelCase`                           | `_handle`, `_debugMessengerOpt`                     |
| constant                                                | `UPPER_SNAKE_CASE`                     | `VALIDATION_LAYERS`, `MAX_SECONDARY_CMD_BUFFERS`    |
| template parameter                                      | `PascalCase`                           | `T`, `Rng`, `Context`                               |
| macro                                                   | `UPPER_SNAKE_CASE` with library prefix | `CVK_RESULT_STABLE_THROW`                           |
| namespace                                               | short lower case                       | `cvk`, `cvk::constants`, `cvk::dev`                 |

- A raw handle in a variable or parameter has the prefix of its API: `vkInstance`, `vmaAllocation`.
- The name says what kind of value it is, a bare object name is the object:
    - index: `<thing>Idx`: `queueIdx`, never only `queue`
    - pointer: `<thing>Ptr`, optional: `<thing>Opt`
    - a name that already says it is no object needs no suffix: `stageId`, `slot`, `handle` (not `handlePtr`)
    - a reference is the object: `cth::ref_wrapper` gets the bare name too: `cmdBuffers`, `_core`
    - loop variable: `i`, `j`, `k` in this order, or `<thing>Idx`
- Names describe the thing, not an invented concept: no `role` for an index.
- `_t` only for aliases that rename a type: `callback_t`, `queue_set_t`. An alias that is a type of its own gets a std
  style name, like `std::string`.
- Declarations leave out parameter names that repeat the type: `wrap(State const&)`, not `VkSemaphore vkSemaphore`.
  Definitions keep them.
- `<lib>::dev` holds implementation details that are not part of the API.

### Language

- Mark every function `constexpr` that can run at compile time, also when nothing uses it there yet.
  A handle owner cannot exist at compile time: its lifecycle is not `constexpr`.
- Put `[[nodiscard]]` on every function that returns a value the caller must use.
- Put `explicit` on constructors with one parameter and on tag constructors.
- Use `auto const` for locals that do not change.
- Initialize aggregates with designated initializers. Vulkan structs always get `.sType` and `.pNext`.
- Data structs (`SubmitInfo`, `Barrier`, info structs) are aggregates. Member functions like `add()` are optional
  shortcuts, checks run at the use.
- A command is a member of the object it acts on: the command buffer for its own state (`cmd.bind(pipeline)`), else the
  object (`barrier.execute(cmd)`).
- Call patterns follow the modern std library conventions.
- Use deducing `this` (`this auto& self`) where a function must work on the derived type.
- Qualify `std::` names. A `.cpp` file can declare `using std::span;` and similar at the top of its namespace.
- No raw `new` and `delete`. Use `std::unique_ptr`, `cth::move_ptr` and `cth::not_null`.
- A non-owning reference that is never null is a `cth::ref_wrapper<T>`: members, data struct fields, lists and
  parameters.
  A type with a conversion operator to `T&` (a jvk wrapper) passes directly. `->` and `*` reach the object.
  `==` of a `cth::ref_wrapper` compares the objects: identity compares the addresses, `&a.get() == &b.get()`.
  A nullable non-owning reference is a `T*` named `<thing>Ptr`. Identity map keys stay `T const*`. Raw Vulkan handles
  stay `vk_not_null`.
- Constructors are public. No `friend` and no passkey class to hide a construction: a friend opens far more than the
  constructor.
- A `friend` only where no other design gives the access, with a comment at the declaration that says why: `StageGraph`
  in `Stage` sets the status of the submission.
- Long or nested template types get a `using` alias next to their use:
  `using last_queue_map_t = cth::dt::flat_hash_map<image_mip_t, cth::ref_wrapper<Queue const>>;`.
- Prefer compile time checks (concepts, `static_assert`) to run time checks.
- An automatically chosen value has an override if:
    - the choice is not guaranteed to be optimal
    - there is a legitimate interest: debugging, user preference
- Never erase types into integers or `void*`. Exception: opaque OS handles are `void*`, like in `cth::os`.

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
- Every library and its tests have a precompiled header in `src/pch/`: `pch.hpp` includes one `glob/<lib>.hpp` per
  external library.
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

    // accessors: created(), get() returns the raw handle (VkSemaphore), ...

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
- The last public section holds what every class has and readers rarely need: copy and move members, `debug_check`
  functions.
- Classes that own a handle are move only.
- The handle lives in `cvk::unique_handle<Handle, Deleter>`: a `std::unique_ptr` whose deleter holds the destroy context
  and calls the global `cvk::destroy`.
  `cvk/utility/handle/` has the deleters and the short forms (`core_handle<VkX>`).
- `cvk/utility/handle/destroy.hpp` declares all overloads of `cvk::destroy`, the .cpp of the object defines its
  overload.
  Reason: the deleters are templates, a qualified name in a template is looked up at its definition.
- Inside a class the member `destroy()` hides the global one: write `cvk::destroy()`.
- Handle and context go in together: `_handle.reset(ptr, core())`. A missing context argument does not compile.
- Move constructor and move assignment are `= default`. The move assignment destroys the old handle with the old
  context.
- A class that owns several objects declares the objects that depend on its handle first, `= default` assigns in that
  order: `_debugMessengerOpt` before `_handle`.
- Classes that do not own their handle (`Queue`, `PhysicalDevice`) keep `cth::move_ptr`.

After the class, in the same header:

```cpp
namespace cvk {
struct Semaphore::State {
    vk_not_null<VkSemaphore> vkSemaphore;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkSemaphore> decompose() && { return {vkSemaphore.get()}; }
};
}

namespace cvk {
inline void Semaphore::debug_check(Semaphore const& semaphore) {
    CTH_CRITICAL(!semaphore.created(), "semaphore must be created") {}
}
}
```

### Object lifecycle

| Function                | Meaning                                                                                        |
|-------------------------|------------------------------------------------------------------------------------------------|
| base constructor        | stores the configuration, does not create a Vulkan object, needs no created parent             |
| `wrap(State)`           | takes ownership of an existing Vulkan object, calls `optDestroy()` first                       |
| `create()`              | creates a new Vulkan object, calls `optDestroy()` first                                        |
| `destroy()`             | destroys the Vulkan object now and forgets, requires `created()`                               |
| `optDestroy()`          | calls `destroy()` if `created()`                                                               |
| `release()`             | gives the `State` to the caller and forgets, does not destroy                                  |
| `State::decompose() &&` | the raw handles of a device state, in destruction order, for the Graveyard                     |
| `destroy()` (global)    | destroys the raw handles, one overload per handle kind, used by the deleters and the Graveyard |
| `forget()`              | sets the members to the not created state, does not destroy                                    |

- The destructor calls `optDestroy()`.
- std and Vulkan names keep their meaning: the lifecycle uses `forget()`, not `reset()`.
- Device objects take `Core const&` in the base constructor. The `core_deleter` of the handle stores it, `core()`
  returns it.
- A parent (instance, core, device, pool, ...) does not move and is not destroyed while objects depend on it. Code does
  not have to support it.
- A failed `create()` calls `forget()` before it throws.
- `release()` exists so that an owner can use an object it must not destroy (for example swapchain images).
- The state is non-owning: the raw handles, the contexts that do not come from the core and the data. A dropped state
  leaks.
- `wrap(State)` takes the context from the core of the object.
- Pooled handles: the state holds the pool, `wrap(State)` has a `CTH_CRITICAL` if it is not the pool of the object.
- Create constructor: the base constructor parameters, then the `create()` parameters. `create()` without required
  parameters -> `create_t` first: `Fence(core, create_t, flags = 0)`.

### Errors

- `CTH_CRITICAL(condition, "message") {}`: precondition check, active in debug builds only.
- `CTH_STABLE_ERR(condition, "message") { throw <lib>::<lib>_exception{details->exception()}; }`: error that can happen
  at run time.
- `CTH_WARN(condition, "message") {}`: suspicious state that is not an error.
- `CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "message") {}`: throws `cvk::vk_result_exception` with the
  `VkResult`.
- A module of cth_engine throws only its exception type `<lib>::<lib>_exception` or a derived type:
  `cvk::cvk_exception`.
- A failed `co_await` of cth's coroutine machinery, also of a jvk awaiter on its bases, throws
  `cth::except::coro_exception` with `code()`. Nobody catches per `co_await`, the exception propagates to the top.
- The condition is the error case: `CTH_CRITICAL(!created(), ...)`, not the expected case.

### Comment markers

- `//TODO` work that is still open
- `//TEMP` code that must change before it is final
- `//IMPLEMENT` a planned change of the design

No space after `//` in markers. Do not put section comments above namespaces.

## Documentation style

Documentation uses doxygen comments and follows ASD-STE100 (Simplified Technical English), adapted to the short style of
this code base.

### Form

- Use `/** ... */` blocks with one tag per line.
- The first line of a block is the brief: what it does, in one line, without a tag. Doxygen reads it with
  `JAVADOC_AUTOBRIEF`.
- Start after the tag with a lower case letter.
- Do not put a period at the end of a line.
- Write short fragments, not full sentences. Start them with a verb in the present tense: `creates the instance`,
  `destroys and forgets`.
- Fragments win over STE100: where STE100 asks for a full sentence or an article, write the shorter fragment.
- Put every piece of code in backticks: our names, Vulkan and std names, values, parameter names, expressions:
  `` `create()` ``, `` `Core::created()` ``, `` `VK_IMAGE_LAYOUT_UNDEFINED` ``, `` `std::nullopt` ``, `` `stageSlot` ``,
  `` `x < y` ``.
  Doxygen links our names inside backticks by itself (autolink), CLion shows backticks as code.
- `@ref` only where autolink does not resolve a name (a member variable is qualified instead:
  `` `Image::State::memory` ``) or where the exact overload matters, see below.
- Write function references without a parameter list: `` `create()` ``, `` `Instance()` ``, `` `Core::queue()` ``.
  `` `create()` `` names the function with all its overloads, it does not mean the overload without parameters.
  Doxygen links the class and the parameter types of `` `Fence(Core const&)` ``, not the overload. The name alone links
  an overload.
  Exception: the text must point at one specific overload: `@ref` with the parameter list links exactly that overload,
  ``@details calls @ref Instance(Config), `wrap()` ``, `@ref create(Config const&)`.
  Do not write `(...)`: it does not link.
- Do not document what the signature already tells. A getter without conditions needs no comment.
- The brief is the purpose. Side effects the caller must know go to `@details`.
- `@details` holds one fact per line, starting with its keyword, so a reader can scan for it. No text walls.
    - one item: inline on the tag line: ``@details calls `optEnd()` on the secondaries``
    - several names: inline on the tag line: ``@details calls @ref Instance(Config), `wrap()` ``
    - several facts: `@details`, optional with a label, then `- ` items
    - one `@details` per topic
- A member with one fact gets a trailing `///<` on its line: `uint32_t framesInFlight; ///< frames in flight, > 0`.
  The line exceeds 100 columns: a one-line `/** fact */` above the member.
  More than one fact: a `/** */` block above it.
- Related functions go to `@see`, not into the text: ``@see `skip()` ``.
- A class brief names what it is, in a few words: `primary command buffer`. Its properties go to `@details` as a list:
  `- owns a command pool`. Copy and move are not documented: the declarations tell it.
- Do not document what Vulkan does anyway: the barriers that Vulkan forces.
  Document what the abstraction adds on top of Vulkan in `@details`: `execute()` appends the resource states of the
  secondaries.
- Document what the caller needs to call it correctly. Nothing about the inside, no examples unless the meaning is
  unclear without one.
- Every `@pre` is clear without the code: name the condition the caller must make true.

### Tags

| Tag          | Use                                                                                                                                                                                                                                                               |
|--------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `@details`   | more information after the brief, one per topic, see the form rules: ``@details calls `create()`, `wrap()` ``                                                                                                                                                     |
| `@param`     | name, then what it is for: `@param config to use`; input is implicit, every output parameter (a non-const reference or pointer that the function writes to) is explicit, also without a description: `@param[out] prologue gets the barriers`, `@param[out] sets` |
| `@see`       | related functions: ``@see `skip()` ``                                                                                                                                                                                                                             |
| `@tparam`    | template parameter, same form as `@param`                                                                                                                                                                                                                         |
| `@return`    | what the result is                                                                                                                                                                                                                                                |
| `@pre`       | condition that must be true before the call: ``@pre `created()` ``                                                                                                                                                                                                |
| `@post`      | condition that is true after the call: ``@post `created()` ``                                                                                                                                                                                                     |
| `@throws`    | exception type and the cause: ``@throws cvk::vk_result_exception result of `vkCreateInstance()` ``; the exception type stays raw, doxygen links it                                                                                                                |
| `@note`      | side effect the caller must know: ``@note calls `optDestroy()` ``                                                                                                                                                                                                 |
| `@attention` | danger that is not a precondition: undefined behavior, not thread safe                                                                                                                                                                                            |

- Write preconditions as `@pre`, not as `@attention requires ...`.
- For an exception of the module itself, write the cause after `reason:`:
  `@throws cvk::cvk_exception reason: required extension missing`.

### Math like notation

Use symbols when they are shorter and clearer than words:

- implication: `` `nullptr` -> no messenger ``, `` `memory == nullptr` -> memory unknown ``
- conditions and ranges: `` @pre `0 < workers < MAX_INT` ``
- special values: `` (`constants::WHOLE_SIZE` => whole buffer) ``
- the expression goes in backticks, the arrow and the words stay outside

### STE100 rules that apply

1. Use one word for one meaning. Do not use synonyms. The terms in the table below are fixed.
2. Write descriptions with 25 words or fewer and instructions with 20 words or fewer.
3. Write one topic in one fragment. Split two actions into two lines.
4. Use the active voice and the present tense.
5. Write instructions to the caller in the imperative: ``call `create()` first``.
6. Do not use phrasal verbs (`set up`, `carry out`). Use one verb (`create`, `do`).
7. Do not use `-ing` forms as nouns or adjectives.
8. Use `it`, `this` and `they` only when the reference is clear.
9. Write the most important condition first, then the consequence.

| Term      | Meaning                                                                                                                     |
|-----------|-----------------------------------------------------------------------------------------------------------------------------|
| construct | the C++ object comes into existence                                                                                         |
| create    | the Vulkan object comes into existence                                                                                      |
| destroy   | the Vulkan object stops to exist                                                                                            |
| wrap      | take ownership of an existing Vulkan object                                                                                 |
| release   | give the ownership to the caller without destroying                                                                         |
| forget    | set the members to the not created state                                                                                    |
| handle    | the raw Vulkan handle, for example `VkInstance`                                                                             |
| state     | the `State` struct: non-owning, the raw handles, the contexts that do not come from the core and the data that `wrap` needs |

### Example

```cpp
/**
 * creates the instance
 * @details a messenger config (see `Config::debugMessenger`) -> creates the messenger
 * @pre volk is initialized
 * @post `created()`
 * @note calls `optDestroy()`
 * @throws cvk::vk_result_exception result of `vkCreateInstance()`
 */
void create();
```

### When you port legacy code

- The legacy code is in the history: `git show 6d013e5:legacy/<path>`.
- Apply this guide to ported code. The legacy macros `cxpr`, `cval` and `declauto` become keywords.
- Check every comment against the code. The legacy comments are not always up to date.
- Report bugs that you find. Do not fix a behavior change silently.

### Repository text

- Do not use em dashes or en dashes in comments, docs, commit messages or markdown.
