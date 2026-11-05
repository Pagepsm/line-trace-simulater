#include "controller/ControllerCompiler.h"
#include "simulation/Simulation.h"
#include <iostream>
#include <stdexcept>
using namespace lt;

void check(bool ok, const std::string& message) {
    if (!ok)
        throw std::runtime_error(message);
}

std::string source(const std::string& body, const std::string& version = "1",
                   const std::string& size = "sizeof(ControllerInput)") {
    return "#include \"controller_api.h\"\n#include <limits>\n"
           "static unsigned ticks=0;\nuint32_t controller_api_version() noexcept{return " +
           version +
           ";}\n"
           "uint32_t controller_input_size() noexcept{return " +
           size +
           ";}\n"
           "uint32_t controller_output_size() noexcept{return sizeof(ControllerOutput);}\n"
           "void controller_reset() noexcept{ticks=0;}\n"
           "uint32_t controller_step(const ControllerInput* in,ControllerOutput* out) noexcept{++ticks;" +
           body + "return 1;}\n";
}

int main() {
    try {
        auto compiler = ControllerCompiler::findCompiler(std::filesystem::current_path());
        check(!compiler.empty(), "GCC not available");
        std::cout << "Compiler: " << compiler.string() << '\n';
        auto build = [&](std::string text) {
            return ControllerCompiler::build(compiler, "test_builds", "controller/controller_api.h",
                                             text);
        };
        auto runtime = std::make_shared<ControllerRuntime>();
        std::string error;
        auto a = build(source("out->left_motor=.3f;out->right_motor=.3f;"));
        check(a.success, a.log);
        check(runtime->load(a.dll, error), error);
        Simulation sim;
        sim.setController(runtime);
        for (int i = 0; i < 240; ++i)
            sim.command(SimulationAction::Step);
        auto straight = sim.body();
        check(straight.pose.position.x > .5f && std::abs(straight.pose.angle) < .001f,
              "DLL A straight motion");
        auto bad = build("#include \"controller_api.h\"\nthis will not compile");
        check(!bad.success && runtime->loaded(), "compile failure retains DLL");
        for (auto invalid :
             {source("out->left_motor=0;", "99"), source("out->left_motor=0;", "1", "1"),
              std::string("extern \"C\" __declspec(dllexport) int unrelated(){return 1;}")}) {
            auto result = build(invalid);
            check(result.success, result.log);
            check(!runtime->load(result.dll, error), "invalid ABI must reject");
            MotorCommand cmd;
            check(runtime->step(0, .01, {0, 1, 1, 0}, cmd, error) && cmd.left == .3f,
                  "invalid DLL must preserve old controller");
        }
        auto b = build(source("out->left_motor=.16f;out->right_motor=.36f;"));
        check(b.success, b.log);
        check(b.dll != a.dll, "unique generation");
        check(runtime->load(b.dll, error), error);
        sim.setController(runtime);
        for (int i = 0; i < 240; ++i)
            sim.command(SimulationAction::Step);
        auto turn = sim.body();
        check(turn.pose.angle > .5f && turn.pose.position.y > -.5f, "DLL B changes actual motion");
        auto stateful =
            build(source("out->left_motor=float(ticks)*.001f;out->right_motor=out->left_motor;"));
        check(stateful.success, stateful.log);
        check(runtime->load(stateful.dll, error), error);
        sim.setController(runtime);
        sim.command(SimulationAction::Step);
        check(sim.motors().left == .001f, "controller starts at first tick");
        sim.command(SimulationAction::Step);
        sim.reset();
        sim.command(SimulationAction::Step);
        check(sim.motors().left == .001f, "reset clears DLL state");
        auto invalidOutput =
            build(source("out->left_motor=std::numeric_limits<float>::quiet_NaN();"));
        check(invalidOutput.success, invalidOutput.log);
        check(runtime->load(invalidOutput.dll, error), error);
        sim.setController(runtime);
        sim.command(SimulationAction::Run);
        sim.advance(.1);
        check(!sim.running() && sim.time() == 0 && !sim.error().empty(),
              "invalid output pauses before physics");
        std::cout << "PASS Controller compile, load, ABI rejection, previous DLL retention, generation switch, state reset, invalid output\n";
        std::cout << "DLL A x=" << straight.pose.position.x << " angle=" << straight.pose.angle
                  << "; DLL B x=" << turn.pose.position.x << " y=" << turn.pose.position.y
                  << " angle=" << turn.pose.angle << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
