#!/usr/bin/env python3
"""
Comprehensive tests for OpenIE Extractor.
Real-time, random monitored tests with varied inputs.
"""

import sys
import json
import random
from scripts.openie_extractor import OpenIEExtractor, ExtractedKnowledge, SPOTriple


def test_basic_extraction():
    """Test basic SPO extraction."""
    extractor = OpenIEExtractor()
    
    test_cases = [
        {
            "text": "Apple Inc. is a technology company founded by Steve Jobs.",
            "topic": "Apple Inc.",
            "expected_triples": 2,
            "checks": [
                lambda r: any("Apple Inc." in t.subject and "technology company" in t.object for t in r.spo_triples),
                lambda r: any("Steve Jobs" in t.object for t in r.spo_triples),
            ]
        },
        {
            "text": "Python is a programming language created by Guido van Rossum.",
            "topic": "Python",
            "expected_triples": 2,
            "checks": [
                lambda r: any("Python" in t.subject and "programming language" in t.object for t in r.spo_triples),
                lambda r: any("Guido van Rossum" in t.object for t in r.spo_triples),
            ]
        },
        {
            "text": "The Eiffel Tower is located in Paris, France. It was built by Gustave Eiffel.",
            "topic": "Eiffel Tower",
            "expected_triples": 2,
            "checks": [
                lambda r: any("Eiffel Tower" in t.subject and "Paris" in t.object for t in r.spo_triples),
                lambda r: any("Gustave Eiffel" in t.object for t in r.spo_triples),
            ]
        },
    ]
    
    results = []
    for i, tc in enumerate(test_cases):
        result = extractor.extract(tc["text"], tc["topic"])
        passed = all(check(result) for check in tc["checks"])
        triple_count = len(result.spo_triples)
        
        results.append({
            "test": f"basic_extraction_{i}",
            "passed": passed,
            "triple_count": triple_count,
            "expected_min": tc["expected_triples"],
            "topic": result.topic,
            "summary": result.summary,
            "triples": [(t.subject, t.predicate, t.object, t.confidence) for t in result.spo_triples]
        })
        
        if not passed:
            print(f"FAIL: test_basic_extraction_{i}")
            print(f"  Text: {tc['text']}")
            print(f"  Triples: {[(t.subject, t.predicate, t.object) for t in result.spo_triples]}")
        else:
            print(f"PASS: test_basic_extraction_{i} ({triple_count} triples)")
    
    return results


def test_complex_sentences():
    """Test extraction from complex sentences."""
    extractor = OpenIEExtractor()
    
    test_cases = [
        {
            "text": "Microsoft, which was founded by Bill Gates and Paul Allen, develops Windows and Office.",
            "topic": "Microsoft",
            "checks": [
                lambda r: any("Microsoft" in t.subject and "Bill Gates" in t.object for t in r.spo_triples),
                lambda r: any("Microsoft" in t.subject and ("Windows" in t.object or "Office" in t.object) for t in r.spo_triples),
            ]
        },
        {
            "text": "Tesla Inc., an electric vehicle manufacturer founded by Elon Musk, produces the Model S and Model 3.",
            "topic": "Tesla Inc.",
            "checks": [
                lambda r: any("Tesla" in t.subject and "Elon Musk" in t.object for t in r.spo_triples),
                lambda r: any("Tesla" in t.subject and "Model" in t.object for t in r.spo_triples),
            ]
        },
        {
            "text": "The theory of relativity, developed by Albert Einstein, revolutionized physics.",
            "topic": "relativity",
            "checks": [
                lambda r: any("relativity" in t.subject.lower() and "Einstein" in t.object for t in r.spo_triples),
            ]
        },
    ]
    
    results = []
    for i, tc in enumerate(test_cases):
        result = extractor.extract(tc["text"], tc["topic"])
        passed = all(check(result) for check in tc["checks"])
        
        results.append({
            "test": f"complex_sentences_{i}",
            "passed": passed,
            "triple_count": len(result.spo_triples),
            "topic": result.topic,
            "triples": [(t.subject, t.predicate, t.object, t.confidence) for t in result.spo_triples]
        })
        
        if not passed:
            print(f"FAIL: test_complex_sentences_{i}")
            print(f"  Text: {tc['text']}")
            print(f"  Triples: {[(t.subject, t.predicate, t.object) for t in result.spo_triples]}")
        else:
            print(f"PASS: test_complex_sentences_{i} ({len(result.spo_triples)} triples)")
    
    return results


def test_passive_voice():
    """Test passive voice extraction."""
    extractor = OpenIEExtractor()
    
    text = "The novel was written by Ernest Hemingway. It was published in 1926."
    result = extractor.extract(text, "novel")
    
    checks = [
        lambda r: any("novel" in t.subject.lower() and "Hemingway" in t.object for t in r.spo_triples),
    ]
    
    passed = all(check(result) for check in checks)
    
    return [{
        "test": "passive_voice",
        "passed": passed,
        "triple_count": len(result.spo_triples),
        "triples": [(t.subject, t.predicate, t.object, t.confidence) for t in result.spo_triples]
    }]


def test_prepositional_relations():
    """Test prepositional phrase extraction."""
    extractor = OpenIEExtractor()
    
    text = "The headquarters of Google is in Mountain View, California. Google operates in many countries."
    result = extractor.extract(text, "Google")
    
    checks = [
        lambda r: any("headquarters" in t.subject.lower() and "Mountain View" in t.object for t in r.spo_triples),
        lambda r: any("Google" in t.subject and "countries" in t.object for t in r.spo_triples),
    ]
    
    passed = all(check(result) for check in checks)
    
    return [{
        "test": "prepositional_relations",
        "passed": passed,
        "triple_count": len(result.spo_triples),
        "triples": [(t.subject, t.predicate, t.object, t.confidence) for t in result.spo_triples]
    }]


def test_entity_extraction():
    """Test named entity extraction."""
    extractor = OpenIEExtractor()
    
    text = "Barack Obama was born in Hawaii. He served as President of the United States."
    result = extractor.extract(text, "Barack Obama")
    
    # Check entities are extracted
    entities = [e.lower() for e in result.entities]
    has_obama = any("obama" in e for e in entities)
    has_hawaii = any("hawaii" in e for e in entities)
    has_united_states = any("united states" in e for e in entities)
    
    passed = has_obama and has_hawaii
    
    return [{
        "test": "entity_extraction",
        "passed": passed,
        "entities": result.entities,
        "has_obama": has_obama,
        "has_hawaii": has_hawaii,
        "has_united_states": has_united_states,
    }]


def test_qa_generation():
    """Test QA pair generation."""
    extractor = OpenIEExtractor()
    
    text = "Python is a programming language. It was created by Guido van Rossum."
    result = extractor.extract(text, "Python")
    
    qa_pairs = result.qa_pairs
    has_what_is = any("what is python" in q.lower() for q, a in qa_pairs)
    has_who_created = any("who created" in q.lower() or "who founded" in q.lower() for q, a in qa_pairs)
    
    passed = has_what_is and has_who_created and len(qa_pairs) > 0
    
    return [{
        "test": "qa_generation",
        "passed": passed,
        "qa_count": len(qa_pairs),
        "qa_pairs": qa_pairs,
        "has_what_is": has_what_is,
        "has_who_created": has_who_created,
    }]


def test_summary_generation():
    """Test summary generation."""
    extractor = OpenIEExtractor()
    
    text = "Machine learning is a subset of artificial intelligence. It enables computers to learn from data. Deep learning is a type of machine learning using neural networks."
    result = extractor.extract(text, "Machine learning")
    
    summary = result.summary.lower()
    has_ml = "machine learning" in summary or "ml" in summary
    has_ai = "artificial intelligence" in summary or "ai" in summary
    
    passed = len(summary) > 20 and (has_ml or has_ai)
    
    return [{
        "test": "summary_generation",
        "passed": passed,
        "summary": result.summary,
        "length": len(result.summary),
    }]


def test_confidence_scores():
    """Test that confidence scores are reasonable."""
    extractor = OpenIEExtractor()
    
    text = "Water boils at 100 degrees Celsius. The sun rises in the east."
    result = extractor.extract(text, "Water")
    
    all_valid = all(0.0 <= t.confidence <= 1.0 for t in result.spo_triples)
    has_high_conf = any(t.confidence > 0.7 for t in result.spo_triples)
    
    passed = all_valid and has_high_conf
    
    return [{
        "test": "confidence_scores",
        "passed": passed,
        "all_valid": all_valid,
        "has_high_conf": has_high_conf,
        "confidences": [t.confidence for t in result.spo_triples],
    }]


def test_minimization():
    """Test MinIE-style minimization removes redundant triples."""
    extractor = OpenIEExtractor()
    
    # Text that could generate redundant extractions
    text = "Apple is a company. Apple Inc. is a technology company. The company was founded by Steve Jobs."
    result = extractor.extract(text, "Apple")
    
    # Should not have both "Apple" and "Apple Inc." as separate subjects for same fact
    # Should not have both "company" and "technology company" for same predicate-object
    subjects = [t.subject for t in result.spo_triples]
    unique_subjects = set(s.lower() for s in subjects)
    
    # Check no duplicate (subject, object) pairs
    pairs = set()
    for t in result.spo_triples:
        key = (t.subject.lower(), t.object.lower())
        pairs.add(key)
    
    passed = len(pairs) == len(result.spo_triples)  # No duplicate pairs
    
    return [{
        "test": "minimization",
        "passed": passed,
        "triple_count": len(result.spo_triples),
        "unique_pairs": len(pairs),
        "subjects": subjects,
    }]


def test_edge_cases():
    """Test edge cases."""
    extractor = OpenIEExtractor()
    
    results = []
    
    # Empty text
    result = extractor.extract("", "test")
    results.append({
        "test": "empty_text",
        "passed": result.topic == "test" and len(result.spo_triples) == 0,
        "topic": result.topic,
    })
    
    # Very short text
    result = extractor.extract("Hi.", "test")
    results.append({
        "test": "short_text",
        "passed": len(result.spo_triples) == 0,
        "triple_count": len(result.spo_triples),
    })
    
    # Text with only stopwords
    result = extractor.extract("The the the and or but.", "test")
    results.append({
        "test": "stopwords_only",
        "passed": len(result.spo_triples) == 0,
        "triple_count": len(result.spo_triples),
    })
    
    # Unicode text
    result = extractor.extract("Tōkyō is the capital of Japan. 東京は日本の首都です。", "Tokyo")
    results.append({
        "test": "unicode_text",
        "passed": len(result.spo_triples) > 0,
        "triple_count": len(result.spo_triples),
        "triples": [(t.subject, t.predicate, t.object) for t in result.spo_triples],
    })
    
    return results


def run_random_monitored_tests():
    """Run randomized tests with varied inputs."""
    extractor = OpenIEExtractor()
    
    # Template sentences with placeholders
    templates = [
        "{entity} is a {type} founded by {founder}.",
        "{entity} develops {product}.",
        "{entity} is located in {location}.",
        "{entity} was created by {founder} in {year}.",
        "{entity} provides {service} to {customer}.",
    ]
    
    entities = ["Google", "Microsoft", "Amazon", "Tesla", "SpaceX", "Meta", "NVIDIA", "Intel"]
    types = ["technology company", "search engine", "cloud provider", "electric vehicle manufacturer", "aerospace company", "social media platform", "AI research company", "semiconductor company"]
    founders = ["Larry Page and Sergey Brin", "Bill Gates", "Jeff Bezos", "Elon Musk", "Mark Zuckerberg", "Jensen Huang", "Robert Noyce"]
    products = ["Android", "Windows", "AWS", "Model S", "Falcon 9", "Facebook", "GPUs", "processors"]
    locations = ["Mountain View, California", "Redmond, Washington", "Seattle, Washington", "Austin, Texas", "Hawthorne, California", "Menlo Park, California", "Santa Clara, California"]
    services = ["search", "cloud computing", "online retail", "electric vehicles", "space launch", "social networking", "AI platforms", "chips"]
    customers = ["users", "developers", "enterprises", "consumers", "researchers"]
    years = ["1998", "1975", "1994", "2003", "2002", "2004", "1993", "1968"]
    
    results = []
    for i in range(20):  # 20 random tests
        template = random.choice(templates)
        text = template.format(
            entity=random.choice(entities),
            type=random.choice(types),
            founder=random.choice(founders),
            product=random.choice(products),
            location=random.choice(locations),
            service=random.choice(services),
            customer=random.choice(customers),
            year=random.choice(years),
        )
        
        result = extractor.extract(text, random.choice(entities))
        
        # Basic validity checks
        valid = (
            isinstance(result.topic, str) and
            isinstance(result.summary, str) and
            isinstance(result.key_facts, list) and
            isinstance(result.qa_pairs, list) and
            isinstance(result.entities, list) and
            isinstance(result.spo_triples, list) and
            all(0 <= t.confidence <= 1 for t in result.spo_triples)
        )
        
        results.append({
            "test": f"random_{i}",
            "passed": valid,
            "text": text[:80] + "...",
            "triple_count": len(result.spo_triples),
            "qa_count": len(result.qa_pairs),
        })
        
        if not valid:
            print(f"FAIL: random_{i}")
            print(f"  Text: {text}")
    
    passed_count = sum(1 for r in results if r["passed"])
    print(f"Random tests: {passed_count}/20 passed")
    
    return results


def main():
    """Run all tests."""
    print("=" * 60)
    print("OpenIE Extractor - Comprehensive Test Suite")
    print("=" * 60)
    
    all_results = []
    
    # Run all test suites
    test_functions = [
        test_basic_extraction,
        test_complex_sentences,
        test_passive_voice,
        test_prepositional_relations,
        test_entity_extraction,
        test_qa_generation,
        test_summary_generation,
        test_confidence_scores,
        test_minimization,
        test_edge_cases,
        run_random_monitored_tests,
    ]
    
    for test_func in test_functions:
        print(f"\n--- Running {test_func.__name__} ---")
        try:
            results = test_func()
            all_results.extend(results)
        except Exception as e:
            print(f"ERROR in {test_func.__name__}: {e}")
            all_results.append({
                "test": test_func.__name__,
                "passed": False,
                "error": str(e)
            })
    
    # Summary
    print("\n" + "=" * 60)
    print("TEST SUMMARY")
    print("=" * 60)
    
    total = len(all_results)
    passed = sum(1 for r in all_results if r.get("passed", False))
    failed = total - passed
    
    print(f"Total tests: {total}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Success rate: {passed/total*100:.1f}%")
    
    if failed > 0:
        print("\nFAILED TESTS:")
        for r in all_results:
            if not r.get("passed", False):
                print(f"  - {r['test']}")
                if "error" in r:
                    print(f"    Error: {r['error']}")
    
    # Save detailed results
    with open("test_results_openie.json", "w") as f:
        json.dump(all_results, f, indent=2)
    
    print(f"\nDetailed results saved to test_results_openie.json")
    
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())