#include "json.hpp"
#include <cstdio>
#include <string>

int main() {
    jx::Value root = jx::Value::object();
    root.set("schemaVersion", jx::Value::integer(1));
    root.set("plate", jx::Value::str("鄂A·12345"));
    root.set("feeCents", jx::Value::integer(1500));

    jx::Value arr = jx::Value::array();
    for (int k = 0; k < 3; ++k) {
        jx::Value e = jx::Value::object();
        e.set("i", jx::Value::integer(k));
        e.set("occupied", jx::Value::boolean(k % 2 == 0));
        arr.push(std::move(e));
    }
    root.set("slots", std::move(arr));

    const std::string text = jx::dump(root, true);
    std::printf("--- dump ---\n%s\n--- size=%zu ---\n", text.c_str(), text.size());

    jx::Value back;
    std::string err;
    if (!jx::parse(text, back, err)) {
        std::printf("PARSE FAILED: %s\n", err.c_str());
        return 1;
    }
    const std::string again = jx::dump(back, true);
    std::printf("roundtrip equal=%d, slots=%zu, plate=%s\n",
                again == text ? 1 : 0,
                back.find("slots") ? back.find("slots")->a.size() : 0,
                back.getString("plate").c_str());
    return again == text ? 0 : 2;
}
