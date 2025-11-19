// Copyright (c) 2020 Pantor. All rights reserved.

#include "inja/environment.hpp"

#include "test-common.hpp"

TEST_CASE("graceful error handling") {
  inja::Environment env;
  inja::json data;
  data["name"] = "Peter";
  data["age"] = 29;
  data["city"] = "Brunswick";

  SUBCASE("disabled by default") {
    // By default, missing variables should throw
    CHECK_THROWS_WITH(env.render("{{unknown}}", data), "[inja.exception.render_error] (at 1:3) variable 'unknown' not found");
    CHECK_THROWS_WITH(env.render("Hello {{ missing_var }}!", data), "[inja.exception.render_error] (at 1:10) variable 'missing_var' not found");
  }

  SUBCASE("enabled graceful errors") {
    env.set_graceful_errors(true);
    
    // Missing variables should render as original text
    CHECK(env.render("{{unknown}}", data) == "{{unknown}}");
    CHECK(env.render("Hello {{ missing_var }}!", data) == "Hello {{ missing_var }}!");
    CHECK(env.render("This prompt contains a {{ bad_variable }}!", data) == "This prompt contains a {{ bad_variable }}!");
    
    // Mixed valid and invalid variables
    CHECK(env.render("{{ name }} lives in {{ unknown_city }}", data) == "Peter lives in {{ unknown_city }}");
    CHECK(env.render("{{ unknown1 }} and {{ name }} and {{ unknown2 }}", data) == "{{ unknown1 }} and Peter and {{ unknown2 }}");
    
    // Valid variables should still work normally
    CHECK(env.render("Hello {{ name }}!", data) == "Hello Peter!");
    CHECK(env.render("{{ name }} is {{ age }} years old.", data) == "Peter is 29 years old.");
  }

  SUBCASE("error tracking") {
    env.set_graceful_errors(true);
    
    // Clear any previous errors
    env.clear_render_errors();
    
    // Render with missing variable
    auto result = env.render("Hello {{ missing_var }}!", data);
    CHECK(result == "Hello {{ missing_var }}!");
    
    // Check that error was tracked
    const auto& errors = env.get_last_render_errors();
    CHECK(errors.size() == 1);
    CHECK(errors[0].message == "variable 'missing_var' not found");
    CHECK(errors[0].original_text == "{{ missing_var }}");
    CHECK(errors[0].location.line == 1);
    CHECK(errors[0].location.column == 10);
  }

  SUBCASE("multiple errors") {
    env.set_graceful_errors(true);
    env.clear_render_errors();
    
    // Render with multiple missing variables
    auto result = env.render("{{ var1 }} and {{ name }} and {{ var2 }} and {{ var3 }}", data);
    CHECK(result == "{{ var1 }} and Peter and {{ var2 }} and {{ var3 }}");
    
    // Check that all errors were tracked
    const auto& errors = env.get_last_render_errors();
    CHECK(errors.size() == 3);
    CHECK(errors[0].message == "variable 'var1' not found");
    CHECK(errors[1].message == "variable 'var2' not found");
    CHECK(errors[2].message == "variable 'var3' not found");
  }

  SUBCASE("nested variables") {
    env.set_graceful_errors(true);
    
    inja::json nested_data;
    nested_data["user"]["name"] = "Alice";
    
    // Missing nested variable
    CHECK(env.render("{{ user.email }}", nested_data) == "{{ user.email }}");
    
    // Missing parent variable
    CHECK(env.render("{{ company.name }}", nested_data) == "{{ company.name }}");
    
    // Valid nested variable
    CHECK(env.render("{{ user.name }}", nested_data) == "Alice");
  }

  SUBCASE("graceful errors with whitespace") {
    env.set_graceful_errors(true);
    
    // Different whitespace patterns should be preserved
    CHECK(env.render("{{  unknown  }}", data) == "{{  unknown  }}");
    CHECK(env.render("{{ unknown}}", data) == "{{ unknown}}");
    CHECK(env.render("{{unknown }}", data) == "{{unknown }}");
  }

  SUBCASE("graceful errors in complex templates") {
    env.set_graceful_errors(true);
    
    std::string tmpl = R"(
Name: {{ name }}
Age: {{ age }}
Email: {{ email }}
City: {{ city }}
Country: {{ country }}
)";
    
    std::string expected = R"(
Name: Peter
Age: 29
Email: {{ email }}
City: Brunswick
Country: {{ country }}
)";
    
    CHECK(env.render(tmpl, data) == expected);
    
    const auto& errors = env.get_last_render_errors();
    CHECK(errors.size() == 2);
  }

  SUBCASE("error clearing") {
    env.set_graceful_errors(true);
    
    // First render with error
    env.render("{{ unknown }}", data);
    CHECK(env.get_last_render_errors().size() == 1);
    
    // Second render without error
    env.render("{{ name }}", data);
    CHECK(env.get_last_render_errors().size() == 0);
    
    // Third render with error again
    env.render("{{ unknown }}", data);
    CHECK(env.get_last_render_errors().size() == 1);
  }
}

TEST_CASE("graceful error handling - null pointer safety") {
  inja::Environment env;
  env.set_graceful_errors(true);
  
  inja::json data;
  data["name"] = "Peter";
  data["items"] = inja::json::array({1, 2, 3});
  data["obj"] = inja::json::object({{"x", 10}, {"y", 20}});

  SUBCASE("unknown function call - null callback") {
    // Calling a function that doesn't exist should not crash
    CHECK_NOTHROW(env.render("{{ unknown_function() }}", data));
    CHECK(env.render("{{ unknown_function() }}", data) == "{{ unknown_function() }}");
    
    CHECK_NOTHROW(env.render("{{ name.unknown_method() }}", data));
    CHECK(env.render("{{ name.unknown_method() }}", data) == "{{ name.unknown_method() }}");
    
    // Should track the error
    const auto& errors = env.get_last_render_errors();
    CHECK(errors.size() >= 1);
  }

  SUBCASE("member access on non-existent members - AtId operation") {
    // Accessing non-existent member should not crash
    CHECK_NOTHROW(env.render("{{ items.append }}", data));
    CHECK(env.render("{{ items.append }}", data) == "{{ items.append }}");
    
    CHECK_NOTHROW(env.render("{{ obj.nonexistent }}", data));
    CHECK(env.render("{{ obj.nonexistent }}", data) == "{{ obj.nonexistent }}");
    
    CHECK_NOTHROW(env.render("{{ name.length }}", data));
    CHECK(env.render("{{ name.length }}", data) == "{{ name.length }}");
  }

  SUBCASE("array access with invalid index - At operation") {
    // Out of bounds array access should not crash
    CHECK_NOTHROW(env.render("{{ at(items, 10) }}", data));
    CHECK_NOTHROW(env.render("{{ at(items, -1) }}", data));
    
    // Valid array access should still work
    CHECK(env.render("{{ at(items, 0) }}", data) == "1");
    CHECK(env.render("{{ at(items, 1) }}", data) == "2");
  }

  SUBCASE("object access with missing key - At operation") {
    // Accessing non-existent key should not crash
    CHECK_NOTHROW(env.render("{{ at(obj, \"z\") }}", data));
    
    // Valid key access should still work
    CHECK(env.render("{{ at(obj, \"x\") }}", data) == "10");
    CHECK(env.render("{{ at(obj, \"y\") }}", data) == "20");
  }

  SUBCASE("chained member access failures") {
    // Chained access on missing members should not crash
    CHECK_NOTHROW(env.render("{{ items.append.call }}", data));
    CHECK(env.render("{{ items.append.call }}", data) == "{{ items.append.call }}");
    
    CHECK_NOTHROW(env.render("{{ missing.nested.deep }}", data));
    CHECK(env.render("{{ missing.nested.deep }}", data) == "{{ missing.nested.deep }}");
  }

  SUBCASE("operations on missing variables") {
    // Operations on missing variables should not crash
    // Note: These may still render as original text since operations fail on null
    auto result1 = env.render("{{ missing + 10 }}", data);
    auto result2 = env.render("{{ missing * 2 }}", data);
    
    // These should work with graceful errors
    CHECK_NOTHROW(env.render("{{ missing == 5 }}", data));
    CHECK_NOTHROW(env.render("{{ missing and true }}", data));
  }

  SUBCASE("function calls with missing arguments") {
    env.add_callback("test_func", 2, [](inja::Arguments& args) {
      // Gracefully handle null arguments
      try {
        return args[0]->get<int>() + args[1]->get<int>();
      } catch (...) {
        return 0;
      }
    });
    
    // Calling function with missing variables should not crash
    CHECK_NOTHROW(env.render("{{ test_func(missing1, missing2) }}", data));
    CHECK_NOTHROW(env.render("{{ test_func(10, missing) }}", data));
  }

  SUBCASE("template assignment with failed operations") {
    // Assignment using failed operations should not crash
    std::string tmpl = R"(
{% set result = items.append %}
{% set value = missing.property %}
{{ name }}
)";
    
    CHECK_NOTHROW(env.render(tmpl, data));
    auto result = env.render(tmpl, data);
    CHECK(result.find("Peter") != std::string::npos);
  }

  SUBCASE("loop over missing array") {
    // Looping over missing variable should not crash
    std::string tmpl = "{% for item in missing %}{{ item }}{% endfor %}Done";
    CHECK_NOTHROW(env.render(tmpl, data));
    CHECK(env.render(tmpl, data) == "Done");
  }

  SUBCASE("conditional with missing variables") {
    // Conditionals with missing variables should not crash
    CHECK_NOTHROW(env.render("{% if missing %}yes{% else %}no{% endif %}", data));
    CHECK_NOTHROW(env.render("{% if missing.property %}yes{% else %}no{% endif %}", data));
  }

  SUBCASE("regression test - array.append crash") {
    // This is the exact pattern that caused the original crash
    // Using .append() on an array (which doesn't exist in inja)
    std::string tmpl = R"(
{% set activeCurses = [] %}
{% set hasEffect = true %}
{% if hasEffect %}
  {% set _ = activeCurses.append({"name": "Test", "type": "test"}) %}
{% endif %}
{{ name }}
)";
    
    // Should not crash, even though .append doesn't exist
    CHECK_NOTHROW(env.render(tmpl, data));
    auto result = env.render(tmpl, data);
    CHECK(result.find("Peter") != std::string::npos);
    
    // Should have tracked the error
    const auto& errors = env.get_last_render_errors();
    CHECK(errors.size() >= 1);
  }

  SUBCASE("regression test - method call on primitive") {
    // Calling methods on primitives should not crash
    CHECK_NOTHROW(env.render("{{ name.toUpperCase() }}", data));
    CHECK_NOTHROW(env.render("{% set x = 5 %}{{ x.toString() }}", data));
    CHECK_NOTHROW(env.render("{% set arr = [1,2,3] %}{{ arr.push(4) }}", data));
  }

  SUBCASE("complex template - multiple checks and array building") {
    // Pattern: Multiple boolean checks stored in variables
    std::string tmpl = R"(
{% set check1 = has_flag("flag1") %}
{% set check2 = has_flag("flag2") %}
{% set check3 = has_flag("flag3") %}

{% set items = [] %}
{% if check1 %}
  {% set _ = items.append({"name": "Item1", "value": 10}) %}
{% endif %}
{% if check2 %}
  {% set _ = items.append({"name": "Item2", "value": 20}) %}
{% endif %}
{% if check3 %}
  {% set _ = items.append({"name": "Item3", "value": 30}) %}
{% endif %}

{% if items | length > 0 %}
Count: {{ items | length }}
{% for item in items %}
- {{ item.name }}: {{ item.value }}
{% endfor %}
{% endif %}
)";
    
    env.add_callback("has_flag", 1, [](inja::Arguments& args) {
      return args[0]->get<std::string>() == "flag1";
    });
    
    // Should not crash, even though .append doesn't work
    CHECK_NOTHROW(env.render(tmpl, data));
    auto result = env.render(tmpl, data);
    // The array will be empty since .append doesn't work, but shouldn't crash
    CHECK(result.find("Peter") == std::string::npos); // Name not in this template
  }

  SUBCASE("complex template - filters on missing values") {
    // Pattern: Using replace and other filters on values
    std::string tmpl = R"(
{% set myArray = [] %}
{% set _ = myArray.append({"type": "test_value", "desc": "Description"}) %}

{% if myArray | length > 0 %}
{% for item in myArray %}
Type: {{ item.type | replace("_", " ") | upper }}
Desc: {{ item.desc | lower }}
{% endfor %}
{% else %}
No items
{% endif %}
)";
    
    CHECK_NOTHROW(env.render(tmpl, data));
    auto result = env.render(tmpl, data);
    CHECK(result.find("No items") != std::string::npos);
  }

  SUBCASE("complex template - nested conditionals with checks") {
    // Pattern: Multiple nested conditions based on flags
    std::string tmpl = R"(
{% set hasFeature1 = check_feature("feature1") %}
{% set hasFeature2 = check_feature("feature2") %}
{% set hasFeature3 = check_feature("feature3") %}

{% if hasFeature1 %}
Feature 1 active
{% endif %}

{% if hasFeature2 %}
Feature 2 active
{% endif %}

{% if hasFeature3 %}
Feature 3 active
{% endif %}

{% if hasFeature1 or hasFeature2 or hasFeature3 %}
At least one feature active
{% else %}
No features active
{% endif %}
)";
    
    env.add_callback("check_feature", 1, [](inja::Arguments& /*args*/) {
      return false; // All return false
    });
    
    CHECK_NOTHROW(env.render(tmpl, data));
    auto result = env.render(tmpl, data);
    CHECK(result.find("No features active") != std::string::npos);
  }

  SUBCASE("array building with object literals") {
    // Pattern: Building arrays with complex objects
    std::string tmpl = R"(
{% set collection = [] %}
{% set flag1 = true %}
{% set flag2 = true %}

{% if flag1 %}
  {% set _ = collection.append({"id": "item1", "category": "type_a", "description": "First item"}) %}
{% endif %}
{% if flag2 %}
  {% set _ = collection.append({"id": "item2", "category": "type_b", "description": "Second item"}) %}
{% endif %}

Total: {{ collection | length }}
{% for entry in collection %}
ID: {{ entry.id }}
Category: {{ entry.category | replace("_", " ") }}
Desc: {{ entry.description }}
{% endfor %}
)";
    
    CHECK_NOTHROW(env.render(tmpl, data));
    // Array will be empty since append doesn't work, but shouldn't crash
    auto result = env.render(tmpl, data);
    CHECK(result.find("Total: 0") != std::string::npos);
  }

  SUBCASE("chained filters on potentially null values") {
    // Pattern: Multiple filters chained together
    std::string tmpl = R"(
{% set items = [] %}
{% set _ = items.append({"field": "test_value_here"}) %}

{% for item in items %}
{{ item.field | replace("_", " ") | upper | length }}
{% endfor %}

Result: {{ missing_var | replace("x", "y") | lower }}
)";
    
    CHECK_NOTHROW(env.render(tmpl, data));
  }

  SUBCASE("conditional rendering with array length checks") {
    // Pattern: Checking array length in multiple places
    std::string tmpl = R"(
{% set statusList = [] %}
{% set condition1 = true %}
{% set condition2 = false %}
{% set condition3 = true %}

{% if condition1 %}{% set _ = statusList.append({"name": "Status1"}) %}{% endif %}
{% if condition2 %}{% set _ = statusList.append({"name": "Status2"}) %}{% endif %}
{% if condition3 %}{% set _ = statusList.append({"name": "Status3"}) %}{% endif %}

{% if statusList | length > 0 %}
Active count: {{ statusList | length }}
{% for status in statusList %}
- {{ status.name }}
{% endfor %}
{% else %}
No active statuses
{% endif %}
)";
    
    CHECK_NOTHROW(env.render(tmpl, data));
    auto result = env.render(tmpl, data);
    CHECK(result.find("No active statuses") != std::string::npos);
  }

  SUBCASE("accessing nested properties with filters") {
    // Pattern: Accessing object properties and applying filters
    std::string tmpl = R"(
{% set records = [] %}
{% set _ = records.append({"status": "active_state", "info": {"nested": "value"}}) %}

{% for record in records %}
Status: {{ record.status | replace("_", " ") }}
Nested: {{ record.info.nested | upper }}
{% endfor %}

Fallback: {{ missing.property | replace("a", "b") }}
)";
    
    CHECK_NOTHROW(env.render(tmpl, data));
  }

  SUBCASE("multiple array operations in sequence") {
    // Pattern: Multiple operations on the same array
    std::string tmpl = R"(
{% set arr = [] %}
{% set _ = arr.append({"val": 1}) %}
{% set _ = arr.append({"val": 2}) %}
{% set _ = arr.append({"val": 3}) %}

Size: {{ arr | length }}
{% if arr | length > 0 %}
First: {{ at(arr, 0).val }}
Last: {{ arr | length }}
{% endif %}
)";
    
    CHECK_NOTHROW(env.render(tmpl, data));
    auto result = env.render(tmpl, data);
    // Array will be empty since append doesn't work
    CHECK(result.find("Size: 0") != std::string::npos);
  }
}

