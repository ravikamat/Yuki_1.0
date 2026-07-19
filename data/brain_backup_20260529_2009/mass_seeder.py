import os

P0_QUEUE = "p0_urgent_queue.txt"
P1_QUEUE = "p1_interest_queue.txt"
P2_QUEUE = "p2_learn_queue.txt"

english_topics = [
    "English grammar", "English language", "Syntax", "Semantics", "Pragmatics",
    "Phonetics", "Phonology", "Morphology (linguistics)", "Vocabulary",
    "English vocabulary", "Idiom", "Figure of speech", "Slang",
    "Parts of speech", "Noun", "Verb", "Adjective", "Adverb", "Pronoun",
    "Preposition", "Conjunction", "Interjection", "Tense", "Aspect",
    "Active voice", "Passive voice", "Sentence clause structure",
    "Punctuation", "Spelling", "Etymology", "Semantics", "Sociolinguistics",
    "Dialect", "Accent (sociolinguistics)", "Standard English",
    "History of English", "Old English", "Middle English", "Modern English",
    "British English", "American English", "Australian English",
    "Canadian English", "Indian English", "South African English",
    "English as a second or foreign language", "Language acquisition",
    "Second-language acquisition", "Linguistics", "Applied linguistics",
    "Reading comprehension", "Writing", "Literature", "English literature",
    "American literature", "Poetry", "Prose", "Drama", "Rhetoric",
    "Stylistics", "Semiotics", "Cognitive linguistics", "Psycholinguistics",
    "Neurolinguistics", "Historical linguistics", "Comparative linguistics",
    "Typology (linguistics)", "Language family", "Indo-European languages"
]

psychology_topics = [
    "Psychology", "Human behavior", "Emotion", "Cognition", "Perception",
    "Learning", "Memory", "Attention", "Motivation", "Personality",
    "Social psychology", "Developmental psychology", "Clinical psychology",
    "Cognitive psychology", "Abnormal psychology", "Educational psychology",
    "Evolutionary psychology", "Health psychology", "Industrial and organizational psychology",
    "Neuropsychology", "Positive psychology", "Quantitative psychology",
    "Sports psychology", "Behaviorism", "Psychoanalysis", "Humanistic psychology",
    "Gestalt psychology", "Cognitive behavioral therapy", "Psychotherapy",
    "Psychiatry", "Neuroscience", "Brain", "Nervous system", "Neuron",
    "Synapse", "Neurotransmitter", "Hormone", "Endocrine system",
    "Mental disorder", "Anxiety disorder", "Mood disorder", "Personality disorder",
    "Psychosis", "Schizophrenia", "Depression (mood)", "Bipolar disorder",
    "Obsessive-compulsive disorder", "Post-traumatic stress disorder",
    "Eating disorder", "Substance abuse", "Addiction", "Stress (biology)",
    "Coping (psychology)", "Resilience (psychology)", "Self-esteem",
    "Self-concept", "Identity (social science)", "Social identity theory",
    "Attachment theory", "Interpersonal relationship", "Love", "Friendship",
    "Family", "Marriage", "Parenting", "Child development", "Adolescence",
    "Adulthood", "Aging", "Death and dying", "Grief", "Happiness",
    "Well-being", "Quality of life", "Empathy", "Compassion", "Altruism",
    "Prosocial behavior", "Aggression", "Violence", "Conflict resolution",
    "Prejudice", "Stereotype", "Discrimination", "Social influence",
    "Conformity", "Obedience", "Persuasion", "Attitude (psychology)"
]

coding_topics = [
    "Computer science", "Computer programming", "Software engineering",
    "Algorithm", "Data structure", "Programming language", "C++", "Python (programming language)",
    "Java (programming language)", "JavaScript", "C (programming language)",
    "C Sharp (programming language)", "PHP", "SQL", "Ruby (programming language)",
    "Swift (programming language)", "Go (programming language)", "Rust (programming language)",
    "Assembly language", "Machine code", "Compiler", "Interpreter (computing)",
    "Operating system", "Computer network", "Database", "Artificial intelligence",
    "Machine learning", "Deep learning", "Neural network", "Natural language processing",
    "Computer vision", "Robotics", "Computer graphics", "Human-computer interaction",
    "Software testing", "Debugging", "Version control", "Git", "GitHub",
    "Agile software development", "Scrum (software development)", "DevOps",
    "Cloud computing", "Cybersecurity", "Cryptography", "Computer architecture",
    "Digital logic", "Boolean algebra", "Discrete mathematics", "Calculus",
    "Linear algebra", "Probability", "Statistics", "Data science",
    "Big data", "Internet of things", "Blockchain", "Quantum computing",
    "Information theory", "Complexity theory", "Turing machine",
    "Object-oriented programming", "Functional programming", "Procedural programming",
    "Declarative programming", "Logic programming", "Design pattern",
    "Sorting algorithm", "Search algorithm", "Graph theory", "Tree (data structure)",
    "Hash table", "Linked list", "Array data structure", "Stack (abstract data type)",
    "Queue (abstract data type)", "Heap (data structure)", "Dynamic programming",
    "Greedy algorithm", "Divide and conquer algorithm", "Backtracking",
    "Recursion", "Iteration", "Variable (computer science)", "Control flow",
    "Data type", "Function (computer programming)", "Object (computer science)",
    "Class (computer programming)", "Method (computer programming)", "Interface (computing)",
    "Module (programming)", "Package (package management system)", "Library (computing)",
    "Framework (software)", "Application programming interface", "Software development kit",
    "Integrated development environment", "Text editor", "Command-line interface",
    "Graphical user interface", "Web development", "Front-end web development",
    "Back-end web development", "Full-stack web development", "Mobile app development"
]

def append_to_queue(filename, topics):
    filepath = os.path.join(os.path.dirname(__file__), filename)
    existing = set()
    if os.path.exists(filepath):
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f:
                existing.add(line.strip().lower())
                
    added = 0
    with open(filepath, "a", encoding="utf-8") as f:
        for t in topics:
            if t.lower() not in existing:
                f.write(t + "\n")
                added += 1
                
    print(f"Added {added} topics to {filename}")

if __name__ == "__main__":
    append_to_queue(P0_QUEUE, english_topics)
    append_to_queue(P1_QUEUE, psychology_topics)
    append_to_queue(P2_QUEUE, coding_topics)
    print("Mass seeding complete. These will act as roots for Wikipedia link traversal.")
