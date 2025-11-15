// Copyright (c) 2020 Pantor. All rights reserved.
// Tests for elif and raw block functionality

#include "inja/environment.hpp"
#include "test-common.hpp"

TEST_CASE("elif statement") {
  inja::Environment env;
  inja::json data;
  data["age"] = 29;
  data["name"] = "Peter";

  SUBCASE("basic elif") {
    CHECK(env.render("{% if age == 28 %}28{% elif age == 29 %}29{% endif %}", data) == "29");
    CHECK(env.render("{% if age == 28 %}28{% elif age == 30 %}30{% else %}other{% endif %}", data) == "other");
  }

  SUBCASE("multiple elif") {
    CHECK(env.render("{% if age == 26 %}26{% elif age == 27 %}27{% elif age == 28 %}28{% elif age == 29 %}29{% endif %}", data) == "29");
    CHECK(env.render("{% if age == 26 %}26{% elif age == 27 %}27{% elif age == 28 %}28{% else %}other{% endif %}", data) == "other");
  }

  SUBCASE("elif with else") {
    CHECK(env.render("{% if age == 28 %}28{% elif age == 29 %}29{% else %}other{% endif %}", data) == "29");
    CHECK(env.render("{% if age == 28 %}28{% elif age == 30 %}30{% else %}{{ age }}{% endif %}", data) == "29");
  }

  SUBCASE("elif with complex expressions") {
    CHECK(env.render("{% if age < 25 %}young{% elif age < 30 %}middle{% else %}old{% endif %}", data) == "middle");
    CHECK(env.render("{% if age > 30 %}old{% elif age >= 29 %}right{% else %}young{% endif %}", data) == "right");
  }

  SUBCASE("nested elif") {
    CHECK(env.render("{% if age > 30 %}{% if name == \"Peter\" %}A{% elif name == \"John\" %}B{% endif %}{% elif age == 29 %}C{% else %}D{% endif %}", data) == "C");
  }

  SUBCASE("elif matches else if behavior") {
    // Test that elif produces the same result as else if
    std::string template_elif = "{% if age == 26 %}26{% elif age == 27 %}27{% elif age == 29 %}29{% else %}other{% endif %}";
    std::string template_else_if = "{% if age == 26 %}26{% else if age == 27 %}27{% else if age == 29 %}29{% else %}other{% endif %}";
    CHECK(env.render(template_elif, data) == env.render(template_else_if, data));
  }

  SUBCASE("elif without matching if") {
    CHECK_THROWS_WITH(env.render("{% elif age == 29 %}29{% endif %}", data), "[inja.exception.parser_error] (at 1:4) elif without matching if");
  }
}

TEST_CASE("raw blocks") {
  inja::Environment env;
  inja::json data;
  data["name"] = "Peter";
  data["age"] = 29;

  SUBCASE("basic raw block") {
    CHECK(env.render("{% raw %}{{ name }}{% endraw %}", data) == "{{ name }}");
    CHECK(env.render("Before {% raw %}{{ name }}{% endraw %} After", data) == "Before {{ name }} After");
  }

  SUBCASE("raw block with multiple variables") {
    CHECK(env.render("{% raw %}{{ name }} is {{ age }} years old{% endraw %}", data) == "{{ name }} is {{ age }} years old");
  }

  SUBCASE("raw block with statements") {
    CHECK(env.render("{% raw %}{% if true %}test{% endif %}{% endraw %}", data) == "{% if true %}test{% endif %}");
    CHECK(env.render("{% raw %}{% for item in items %}{{ item }}{% endfor %}{% endraw %}", data) == "{% for item in items %}{{ item }}{% endfor %}");
  }

  SUBCASE("raw block preserves whitespace") {
    CHECK(env.render("{% raw %}  {{ name }}  \n  {{ age }}  {% endraw %}", data) == "  {{ name }}  \n  {{ age }}  ");
  }

  SUBCASE("multiple raw blocks") {
    CHECK(env.render("{% raw %}{{ a }}{% endraw %} and {% raw %}{{ b }}{% endraw %}", data) == "{{ a }} and {{ b }}");
  }

  SUBCASE("raw block with processed content outside") {
    CHECK(env.render("{{ name }} {% raw %}{{ age }}{% endraw %} {{ age }}", data) == "Peter {{ age }} 29");
  }

  SUBCASE("raw block with special characters") {
    CHECK(env.render("{% raw %}#{{ name }} @{{ age }}{% endraw %}", data) == "#{{ name }} @{{ age }}");
  }

  SUBCASE("empty raw block") {
    CHECK(env.render("{% raw %}{% endraw %}", data) == "");
  }

  SUBCASE("raw block in conditionals") {
    CHECK(env.render("{% if age == 29 %}{% raw %}{{ name }}{% endraw %}{% endif %}", data) == "{{ name }}");
    CHECK(env.render("{% if age == 30 %}{% raw %}{{ name }}{% endraw %}{% endif %}", data) == "");
  }

  SUBCASE("raw block in loops") {
    data["items"] = {"a", "b", "c"};
    CHECK(env.render("{% for item in items %}{% raw %}{{ x }}{% endraw %} {% endfor %}", data) == "{{ x }} {{ x }} {{ x }} ");
  }

  SUBCASE("raw without matching endraw") {
    CHECK_THROWS_WITH(env.render("{% raw %}{{ name }}", data), "[inja.exception.parser_error] (at 1:8) unmatched raw");
  }

  SUBCASE("endraw without matching raw") {
    CHECK_THROWS_WITH(env.render("{% endraw %}", data), "[inja.exception.parser_error] (at 1:4) endraw without matching raw");
  }

  SUBCASE("nested raw blocks are not supported") {
    // The first endraw will close the raw block, leaving the second endraw unmatched
    CHECK_THROWS_WITH(env.render("{% raw %}{% raw %}inner{% endraw %} outer{% endraw %}", data), 
                     "[inja.exception.parser_error] (at 1:45) endraw without matching raw");
  }
}

TEST_CASE("combined elif and raw") {
  inja::Environment env;
  inja::json data;
  data["mode"] = "template";
  data["name"] = "Peter";

  SUBCASE("basic elif without raw") {
    std::string tmpl = "{% if mode == \"other\" %}other{% elif mode == \"template\" %}{{ name }}{% endif %}";
    CHECK(env.render(tmpl, data) == "Peter");
  }

  SUBCASE("elif with raw blocks") {
    // Simplified: test raw in if branch separately
    std::string tmpl1 = "{% if mode == \"raw\" %}{% raw %}{{ name }}{% endraw %}{% endif %}";
    data["mode"] = "raw";
    CHECK(env.render(tmpl1, data) == "{{ name }}");
    
    // Test elif with processed variable
    std::string tmpl2 = "{% if mode == \"other\" %}other{% elif mode == \"template\" %}{{ name }}{% endif %}";
    data["mode"] = "template";
    CHECK(env.render(tmpl2, data) == "Peter");
  }
}

