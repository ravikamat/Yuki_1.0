import sys, os, time
import threading

sys.path.append("data/brain")
from yuki_knowledge_daemon import KnowledgeDaemon

daemon = KnowledgeDaemon()

print("Initial DB Size:", daemon._facts_count)

topics_to_learn = []
for i in range(100000):
    topics_to_learn.append(f"Mock_Topic_{i}")

mock_summary = "This is a mock summary for a very interesting topic. " \
               "It is designed to contain several sentences, including some keywords like " \
               "artificial intelligence, psychology, emotion, and science. " \
               "We will insert this thousands of times to test sqlite WAL scaling and TF-IDF memory mapping."

print("Starting bulk insert of 100,000 topics...")

def worker(start_idx, end_idx, tid):
    inserted = 0
    for i in range(start_idx, end_idx):
        topic = topics_to_learn[i]
        # _store_fact expects topic, summary, source
        success = daemon._store_fact(topic, mock_summary + f" Unique id {i}.", "test")
        if success: inserted += 1
        if i % 2500 == 0:
            print(f"[Thread {tid}] Reached index {i}...")
    print(f"[Thread {tid}] Done. Inserted {inserted}")

threads = []
chunk_size = 10000
t0 = time.time()

for i in range(10):
    start = i * chunk_size
    end = start + chunk_size
    t = threading.Thread(target=worker, args=(start, end, i))
    t.start()
    threads.append(t)

for t in threads:
    t.join()

dt = time.time() - t0
print(f"\nCompleted. Inserted 100,000 topics in {dt:.2f} seconds.")

# Now test a query
print("\nTesting Query performance on 100K+ rows:")
t1 = time.time()
ans = daemon._query("what is psychology and emotion")
dt_query = time.time() - t1
print(f"Query returned in {dt_query*1000:.2f} ms")
print("Found:", ans.get("found"))
