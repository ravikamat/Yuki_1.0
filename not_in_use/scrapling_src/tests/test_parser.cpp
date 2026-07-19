#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include "parser/html_parser.hpp"
#include "parser/selector.hpp"
#include "parser/css_selector.hpp"

using namespace scrapling;

static int passed = 0, failed = 0;

#define TEST(name) std::cout << "[TEST] " << name << " ... ";
#define PASS() { passed++; std::cout << "PASS\n"; }
#define FAIL(msg) { failed++; std::cerr << "FAIL: " << msg << "\n"; }

void test_html_parser() {
    TEST("HTML parser basic");
    HtmlParser parser;
    auto doc = parser.parse("<html><head><title>Test</title></head><body><h1>Hello</h1><p>World</p></body></html>");
    if (!doc.root) { FAIL("root is null"); return; }
    if (doc.title() != "Test") { FAIL("title mismatch: " + doc.title()); return; }
    auto body = doc.body();
    if (!body) { FAIL("body not found"); return; }
    auto h1 = body->find("h1");
    if (!h1) { FAIL("h1 not found"); return; }
    if (h1->inner_text() != "Hello") { FAIL("h1 text mismatch: " + h1->inner_text()); return; }
    PASS();
}

void test_css_selector() {
    TEST("CSS selector tag");
    HtmlParser parser;
    auto doc = parser.parse("<div><p class='a'>1</p><p class='b'>2</p><span>3</span></div>");
    auto ps = doc.root->find_all("p");
    if (ps.size() != 2) { FAIL("expected 2 p, got " + std::to_string(ps.size())); return; }
    PASS();

    TEST("CSS selector class");
    auto a = doc.root->find_all(".a");
    if (a.size() != 1) { FAIL("expected 1 .a, got " + std::to_string(a.size())); return; }
    if (a[0]->inner_text() != "1") { FAIL("text mismatch"); return; }
    PASS();

    TEST("CSS selector id");
    auto doc2 = parser.parse("<div id='foo'>bar</div>");
    auto foo = doc2.root->find_all("#foo");
    if (foo.size() != 1) { FAIL("expected 1 #foo"); return; }
    PASS();

    TEST("CSS selector descendant");
    auto doc3 = parser.parse("<div><span><a href='x'>link</a></span></div><a href='y'>other</a>");
    auto links = doc3.root->find_all("div a");
    if (links.size() != 1) { FAIL("expected 1 div a, got " + std::to_string(links.size())); return; }
    PASS();

    TEST("CSS selector child");
    auto doc4 = parser.parse("<div><span>child</span><p><span>grandchild</span></p></div>");
    auto spans = doc4.root->find_all("div > span");
    if (spans.size() != 1) { FAIL("expected 1 div > span, got " + std::to_string(spans.size())); return; }
    PASS();

    TEST("CSS selector attribute");
    auto doc5 = parser.parse("<input type='text' name='foo'><input type='password' name='bar'>");
    auto inputs = doc5.root->find_all("input[type='text']");
    if (inputs.size() != 1) { FAIL("expected 1 text input"); return; }
    PASS();

    TEST("CSS selector nth-child");
    auto doc6 = parser.parse("<ul><li>1</li><li>2</li><li>3</li></ul>");
    auto second = doc6.root->find_all("li:nth-child(2)");
    if (second.size() != 1) { FAIL("expected 1 li:nth-child(2)"); return; }
    if (second[0]->inner_text() != "2") { FAIL("text mismatch"); return; }
    PASS();

    TEST("CSS selector pseudo-class first-child");
    auto first = doc6.root->find_all("li:first-child");
    if (first.size() != 1) { FAIL("expected 1 first-child"); return; }
    if (first[0]->inner_text() != "1") { FAIL("text mismatch"); return; }
    PASS();

    TEST("CSS selector pseudo-class last-child");
    auto last = doc6.root->find_all("li:last-child");
    if (last.size() != 1) { FAIL("expected 1 last-child"); return; }
    if (last[0]->inner_text() != "3") { FAIL("text mismatch"); return; }
    PASS();

    TEST("CSS selector adjacent sibling");
    auto doc7 = parser.parse("<h1>Title</h1><p>Para1</p><p>Para2</p><div>Div</div>");
    auto adj = doc7.root->find_all("h1 + p");
    if (adj.size() != 1) { FAIL("expected 1 h1 + p"); return; }
    if (adj[0]->inner_text() != "Para1") { FAIL("text mismatch"); return; }
    PASS();

    TEST("CSS selector general sibling");
    auto gen = doc7.root->find_all("h1 ~ p");
    if (gen.size() != 2) { FAIL("expected 2 h1 ~ p, got " + std::to_string(gen.size())); return; }
    PASS();

    TEST("CSS selector multiple classes");
    auto doc8 = parser.parse("<div class='a b c'>x</div><div class='a'>y</div>");
    auto abc = doc8.root->find_all(".a.b");
    if (abc.size() != 1) { FAIL("expected 1 .a.b, got " + std::to_string(abc.size())); return; }
    PASS();

    TEST("CSS selector :has()");
    auto doc9 = parser.parse("<div><a href='x'>link</a></div><div><span>no link</span></div>");
    auto has_a = doc9.root->find_all("div:has(a)");
    if (has_a.size() != 1) { FAIL("expected 1 div:has(a)"); return; }
    PASS();

    TEST("CSS selector :not()");
    auto doc10 = parser.parse("<div class='a'>1</div><div class='b'>2</div>");
    auto not_a = doc10.root->find_all("div:not(.a)");
    if (not_a.size() != 1) { FAIL("expected 1 div:not(.a)"); return; }
    if (not_a[0]->inner_text() != "2") { FAIL("text mismatch"); return; }
    PASS();
}

void test_selector_api() {
    TEST("Selector API");
    Selector sel("<html><body><div class='item'><h2>Title</h2><p>Desc</p><a href='/link'>Click</a></div></body></html>", "https://example.com/");

    auto item = sel.css(".item");
    if (!item) { FAIL(".item not found"); return; }

    auto h2 = item->css("h2");
    if (!h2) { FAIL("h2 not found"); return; }
    if (h2->text().raw() != "Title") { FAIL("h2 text mismatch"); return; }

    auto link = item->css("a");
    if (!link) { FAIL("a not found"); return; }
    if (link->href() != "https://example.com/link") { FAIL("href mismatch: " + link->href()); return; }

    auto all_p = item->css_all("p");
    if (all_p.size() != 1) { FAIL("expected 1 p"); return; }

    PASS();

    TEST("Selector table extraction");
    Selector table_sel("<table><tr><th>Name</th><th>Age</th></tr><tr><td>Alice</td><td>30</td></tr><tr><td>Bob</td><td>25</td></tr></table>");
    auto table = table_sel.css("table");
    if (!table) { FAIL("table not found"); return; }
    auto data = table->table();
    if (data.size() != 3) { FAIL("expected 3 rows, got " + std::to_string(data.size())); return; }
    if (data[0][0] != "Name") { FAIL("header mismatch"); return; }
    if (data[1][1] != "30") { FAIL("data mismatch"); return; }
    PASS();

    TEST("Selector JSON-LD");
    Selector jsonld_sel(R"(<script type="application/ld+json">{"@context":"https://schema.org","@type":"Product","name":"Widget"}</script>)");
    auto jld = jsonld_sel.json_ld();
    if (jld.size() != 1) { FAIL("expected 1 JSON-LD"); return; }
    if (jld[0]["@type"] != "Product") { FAIL("type mismatch"); return; }
    PASS();

    TEST("Selector clean_text");
    Selector text_sel("<p>  Hello   \n\n  World  \t  </p>");
    auto clean = text_sel.css("p")->clean_text();
    if (clean != "Hello World") { FAIL("clean_text mismatch: '" + clean + "'"); return; }
    PASS();

    TEST("Selector attr");
    Selector attr_sel("<img src='pic.jpg' alt='Picture' width='100'>");
    auto img = attr_sel.css("img");
    if (!img) { FAIL("img not found"); return; }
    auto src = img->attr("src");
    if (!src || *src != "pic.jpg") { FAIL("src mismatch"); return; }
    PASS();

    TEST("Selector xpath simple");
    Selector xpath_sel("<root><item id='1'>A</item><item id='2'>B</item></root>");
    auto items = xpath_sel.xpath_all("//item");
    if (items.size() != 2) { FAIL("expected 2 items, got " + std::to_string(items.size())); return; }
    PASS();
}

void test_url_utils() {
    TEST("URL join absolute");
    if (url::join("https://example.com/path", "https://other.com/") != "https://other.com/") {
        FAIL("absolute URL not preserved"); return;
    }
    PASS();

    TEST("URL join relative path");
    if (url::join("https://example.com/a/b/c", "d.html") != "https://example.com/a/b/d.html") {
        FAIL("relative path join failed: " + url::join("https://example.com/a/b/c", "d.html")); return;
    }
    PASS();

    TEST("URL join absolute path");
    if (url::join("https://example.com/a/b", "/c/d") != "https://example.com/c/d") {
        FAIL("absolute path join failed"); return;
    }
    PASS();

    TEST("URL domain");
    if (url::domain("https://sub.example.com:8080/path") != "sub.example.com") {
        FAIL("domain extraction failed: " + url::domain("https://sub.example.com:8080/path")); return;
    }
    PASS();

    TEST("URL is_absolute");
    if (!url::is_absolute("https://example.com")) { FAIL("absolute check failed"); return; }
    if (url::is_absolute("/path")) { FAIL("relative check failed"); return; }
    PASS();
}

void test_edge_cases() {
    TEST("Self-closing tags");
    HtmlParser parser;
    auto doc = parser.parse("<div><img src='x.jpg'><br><input type='text'></div>");
    auto imgs = doc.root->find_all("img");
    if (imgs.size() != 1) { FAIL("expected 1 img"); return; }
    auto brs = doc.root->find_all("br");
    if (brs.size() != 1) { FAIL("expected 1 br"); return; }
    PASS();

    TEST("Nested tables");
    auto doc2 = parser.parse("<table><tr><td><table><tr><td>inner</td></tr></table></td></tr></table>");
    auto tables = doc2.root->find_all("table");
    if (tables.size() != 2) { FAIL("expected 2 tables, got " + std::to_string(tables.size())); return; }
    PASS();

    TEST("Script tag raw text");
    auto doc3 = parser.parse("<script>var x = '<div>not html</div>';</script>");
    auto scripts = doc3.root->find_all("script");
    if (scripts.size() != 1) { FAIL("expected 1 script"); return; }
    if (scripts[0]->inner_text().find("<div>") == std::string::npos) {
        FAIL("script content mangled: " + scripts[0]->inner_text()); return;
    }
    PASS();

    TEST("Comments");
    auto doc4 = parser.parse("<div><!-- comment --><p>real</p></div>");
    auto ps = doc4.root->find_all("p");
    if (ps.size() != 1) { FAIL("expected 1 p after comment"); return; }
    PASS();

    TEST("Empty document");
    auto doc5 = parser.parse("");
    if (!doc5.root) { FAIL("empty doc should have root"); return; }
    PASS();

    TEST("Malformed HTML");
    auto doc6 = parser.parse("<p>unclosed<div><span>nested</div>");
    auto spans = doc6.root->find_all("span");
    if (spans.size() != 1) { FAIL("expected 1 span in malformed"); return; }
    PASS();

    TEST("Case insensitive tags");
    auto doc7 = parser.parse("<DIV><P>Text</P></DIV>");
    auto divs = doc7.root->find_all("div");
    if (divs.size() != 1) { FAIL("expected 1 div (case insensitive)"); return; }
    PASS();

    TEST("Complex selector");
    auto doc8 = parser.parse(
        "<article class='post' data-id='42'>"
        "<header><h1 class='title'>Hello</h1></header>"
        "<div class='content'><p class='intro'>Intro</p><p>Body</p></div>"
        "<footer><span class='author'>Alice</span></footer>"
        "</article>"
    );
    auto article = doc8.root->find("article.post[data-id='42']");
    if (!article) { FAIL("complex selector failed"); return; }
    auto author = article->find("footer .author");
    if (!author || author->inner_text() != "Alice") { FAIL("nested selector failed"); return; }
    PASS();

    TEST("Selector siblings");
    auto doc9 = parser.parse("<ul><li>1</li><li>2</li><li>3</li></ul>");
    auto li2 = doc9.root->find("li:nth-child(2)");
    if (!li2) { FAIL("li2 not found"); return; }
    auto next = li2->next_element_sibling();
    if (!next || next->inner_text() != "3") { FAIL("next sibling failed"); return; }
    auto prev = li2->prev_element_sibling();
    if (!prev || prev->inner_text() != "1") { FAIL("prev sibling failed"); return; }
    PASS();
}

int main() {
    std::cout << "=== Scrapling C++ Parser Tests ===\n\n";

    test_html_parser();
    test_css_selector();
    test_selector_api();
    test_url_utils();
    test_edge_cases();

    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Total:  " << (passed + failed) << "\n";

    return failed > 0 ? 1 : 0;
}
