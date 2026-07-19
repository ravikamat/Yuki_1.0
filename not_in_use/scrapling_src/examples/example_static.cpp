#include <iostream>
#include "parser/selector.hpp"

using namespace scrapling;

int main() {
    // Parse HTML from string
    std::string html = R"(
        <html>
        <head><title>Product Page</title></head>
        <body>
            <div class="product" data-sku="ABC123">
                <h1 class="title">Super Widget</h1>
                <p class="price">$29.99</p>
                <div class="description">
                    <p>This is an amazing widget.</p>
                    <p>Buy it now!</p>
                </div>
                <a href="/cart/add/ABC123" class="buy-btn">Add to Cart</a>
                <table class="specs">
                    <tr><th>Weight</th><th>Color</th></tr>
                    <tr><td>1.5kg</td><td>Blue</td></tr>
                </table>
            </div>
            <div class="reviews">
                <div class="review">
                    <span class="author">Alice</span>
                    <span class="rating">5</span>
                    <p>Great product!</p>
                </div>
                <div class="review">
                    <span class="author">Bob</span>
                    <span class="rating">4</span>
                    <p>Good value.</p>
                </div>
            </div>
        </body>
        </html>
    )";

    Selector page(html, "https://shop.example.com/products/ABC123");

    // Extract product info
    auto product = page.css(".product");
    if (!product) {
        std::cerr << "Product not found!\n";
        return 1;
    }

    std::cout << "Title: " << product->css(".title")->text().raw() << "\n";
    std::cout << "Price: " << product->css(".price")->text().raw() << "\n";
    std::cout << "SKU: " << product->attr("data-sku").value_or("N/A") << "\n";

    // Extract description paragraphs
    auto desc = product->css(".description");
    if (desc) {
        std::cout << "\nDescription:\n";
        for (const auto& p : desc->css_all("p")) {
            std::cout << "  - " << p->text().raw() << "\n";
        }
    }

    // Extract buy link
    auto buy = product->css(".buy-btn");
    if (buy) {
        std::cout << "\nBuy URL: " << buy->href() << "\n";
    }

    // Extract specs table
    auto specs = product->css(".specs");
    if (specs) {
        std::cout << "\nSpecs:\n";
        auto table = specs->table();
        for (const auto& row : table) {
            for (const auto& cell : row) {
                std::cout << "  " << cell;
            }
            std::cout << "\n";
        }
    }

    // Extract all reviews
    auto reviews = page.css_all(".review");
    std::cout << "\nReviews (" << reviews.size() << "):\n";
    for (const auto& review : reviews) {
        auto author = review->css(".author");
        auto rating = review->css(".rating");
        auto text = review->css("p");

        std::cout << "  " << (author ? author->text().raw() : "Unknown")
                  << " (" << (rating ? rating->text().raw() : "?") << "/5): "
                  << (text ? text->text().raw() : "") << "\n";
    }

    // XPath example
    std::cout << "\nXPath //span[@class='rating']:\n";
    auto ratings = page.xpath_all("//span[@class='rating']");
    for (const auto& r : ratings) {
        std::cout << "  Rating: " << r->text().raw() << "\n";
    }

    return 0;
}
