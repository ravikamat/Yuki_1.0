#!/usr/bin/env python3
"""
OpenIE Extractor for Yuki - Best-in-class (subject, predicate, object) extraction.
Uses NLTK for POS tagging and linguistic patterns to extract SPO triples.
Implements MinIE-style minimization to avoid over-specified extractions.
"""

import json
import sys
import re
from typing import List, Dict, Tuple, Optional, Set
from dataclasses import dataclass, asdict

import nltk
from nltk import pos_tag, word_tokenize, ne_chunk
from nltk.tree import Tree


@dataclass
class SPOTriple:
    """Subject-Predicate-Object triple with confidence."""
    subject: str
    predicate: str
    object: str
    confidence: float
    source_sentence: str


@dataclass
class ExtractedKnowledge:
    """Structured knowledge extracted from text."""
    topic: str
    summary: str
    key_facts: List[str]
    qa_pairs: List[Tuple[str, str]]
    entities: List[str]
    spo_triples: List[SPOTriple]
    json_ld: str


class OpenIEExtractor:
    """
    High-quality OpenIE extractor using NLTK.
    Extracts (subject, predicate, object) triples from natural language text.
    Implements MinIE-style minimization for clean extractions.
    """

    def __init__(self):
        self._init_patterns()
        self._init_stopwords()

    def _init_patterns(self):
        """Initialize linguistic patterns for SPO extraction."""
        self.verb_tags = {'VB', 'VBD', 'VBG', 'VBN', 'VBP', 'VBZ', 'MD'}
        self.noun_tags = {'NN', 'NNS', 'NNP', 'NNPS', 'PRP', 'PRP$'}
        self.adj_tags = {'JJ', 'JJR', 'JJS'}
        self.adv_tags = {'RB', 'RBR', 'RBS', 'WRB'}

        self.copula_verbs = {'be', 'is', 'are', 'was', 'were', 'been', 'being', 'am'}
        self.light_verbs = {'have', 'has', 'had', 'do', 'does', 'did', 'make', 'made', 'take', 'took', 'give', 'gave'}

    def _init_stopwords(self):
        """Initialize stopwords for entity filtering."""
        self.stopwords = {
            'the', 'a', 'an', 'and', 'or', 'but', 'in', 'on', 'at', 'to', 'for',
            'of', 'with', 'by', 'from', 'up', 'about', 'into', 'through', 'during',
            'before', 'after', 'above', 'below', 'between', 'among', 'this', 'that',
            'these', 'those', 'i', 'you', 'he', 'she', 'it', 'we', 'they', 'me',
            'him', 'her', 'us', 'them', 'my', 'your', 'his', 'its', 'our', 'their',
            'is', 'are', 'was', 'were', 'be', 'been', 'being', 'have', 'has', 'had',
            'do', 'does', 'did', 'will', 'would', 'could', 'should', 'may', 'might',
            'must', 'can', 'shall', 'not', 'no', 'yes', 'so', 'as', 'if', 'then',
            'than', 'when', 'where', 'why', 'how', 'what', 'who', 'which', 'whose',
            'there', 'here', 'now', 'then', 'also', 'just', 'only', 'even', 'still'
        }

    def extract(self, text: str, topic_hint: str = "") -> ExtractedKnowledge:
        """Main extraction pipeline."""
        if not text or not text.strip():
            return ExtractedKnowledge(
                topic=topic_hint, summary="", key_facts=[], qa_pairs=[],
                entities=[], spo_triples=[], json_ld=""
            )

        sentences = self._split_sentences(text)
        all_triples = []
        all_entities = set()
        key_facts = []

        for sent in sentences:
            if len(sent.strip()) < 20:
                continue

            try:
                tokens = word_tokenize(sent)
                tagged = pos_tag(tokens)
                chunks = ne_chunk(tagged)
            except Exception:
                continue

            sent_entities = self._extract_entities(chunks)
            all_entities.update(sent_entities)

            triples = self._extract_spo_from_sentence(tagged, sent)
            all_triples.extend(triples)

            for t in triples:
                all_entities.add(t.subject)
                all_entities.add(t.object)

            if len(sent) > 50:
                key_facts.append(sent)

        minimized_triples = self._minimize_triples(all_triples)
        qa_pairs = self._triples_to_qa(minimized_triples)
        summary = self._generate_summary(minimized_triples, key_facts, topic_hint)
        topic = topic_hint or self._extract_topic(text, minimized_triples)

        return ExtractedKnowledge(
            topic=topic, summary=summary, key_facts=key_facts[:5],
            qa_pairs=qa_pairs, entities=sorted(list(all_entities))[:20],
            spo_triples=minimized_triples, json_ld=""
        )

    def _split_sentences(self, text: str) -> List[str]:
        try:
            return nltk.sent_tokenize(text)
        except Exception:
            return re.split(r'[.!?]+\s+', text)

    def _extract_spo_from_sentence(self, tagged: List[Tuple[str, str]], sentence: str) -> List[SPOTriple]:
        triples = []
        try:
            tokens = word_tokenize(sentence)
            tagged = pos_tag(tokens)
            chunks = ne_chunk(tagged)
        except Exception:
            return triples

        entities = self._extract_entities(chunks)
        svc_triples = self._extract_svc(tagged, sentence)
        triples.extend(svc_triples)

        triples.extend(self._extract_relative_clauses(tagged, sentence))
        triples.extend(self._extract_svo(tagged, sentence))
        triples.extend(self._extract_passive(tagged, sentence))
        triples.extend(self._extract_prepositional(tagged, sentence))
        triples.extend(self._extract_apposition(tagged, sentence))
        triples.extend(self._extract_apposition_with_verb(tagged, sentence))
        triples.extend(self._propagate_subject_to_participles(tagged, sentence, svc_triples))
        return triples

    def _extract_entities(self, chunks: Tree) -> Set[str]:
        entities = set()
        for subtree in chunks:
            if isinstance(subtree, Tree) and subtree.label() in ('PERSON', 'ORGANIZATION', 'GPE', 'LOCATION', 'FACILITY'):
                entity = ' '.join([token for token, _ in subtree.leaves()])
                entities.add(entity)
        return entities

    def _extract_svc(self, tagged: List[Tuple[str, str]], sentence: str) -> List[SPOTriple]:
        triples = []
        for i, (word, tag) in enumerate(tagged):
            if word.lower() in self.copula_verbs and tag in self.verb_tags:
                subject = self._find_subject(tagged, i)
                complement = self._find_complement(tagged, i)
                if subject and complement:
                    predicate = 'is'
                    confidence = self._compute_confidence(subject, predicate, complement, 'SVC')
                    triples.append(SPOTriple(subject, predicate, complement, confidence, sentence))
        return triples

    def _extract_passive(self, tagged: List[Tuple[str, str]], sentence: str) -> List[SPOTriple]:
        triples = []
        for i, (word, tag) in enumerate(tagged):
            if tag == 'VBN' and i > 0 and tagged[i-1][0].lower() in self.copula_verbs:
                by_phrase = self._find_by_phrase(tagged, i)
                if by_phrase:
                    obj = self._find_subject(tagged, i)
                    if obj:
                        predicate = f"is {word} by"
                        confidence = self._compute_confidence(obj, predicate, by_phrase, 'PASSIVE')
                        triples.append(SPOTriple(obj, predicate, by_phrase, confidence, sentence))
        return triples

    def _extract_svo(self, tagged: List[Tuple[str, str]], sentence: str) -> List[SPOTriple]:
        triples = []
        for i, (word, tag) in enumerate(tagged):
            if tag in self.verb_tags and word.lower() not in self.copula_verbs:
                if tag == 'VBN' and i > 0 and tagged[i-1][1] in self.noun_tags:
                    continue
                subject = self._find_subject(tagged, i)
                obj = self._find_object(tagged, i)
                if subject and obj:
                    predicate = self._normalize_predicate(word, tagged, i)
                    confidence = self._compute_confidence(subject, predicate, obj, 'SVO')
                    triples.append(SPOTriple(subject, predicate, obj, confidence, sentence))
        return triples

    def _extract_relative_clauses(self, tagged: List[Tuple[str, str]], sentence: str) -> List[SPOTriple]:
        triples = []
        relative_pronouns = {'which', 'who', 'whom', 'that'}
        for i, (word, tag) in enumerate(tagged):
            if word.lower() in relative_pronouns and tag in {'WDT', 'WP', 'WP$', 'IN'}:
                for j in range(i + 1, min(i + 6, len(tagged))):
                    w2, t2 = tagged[j]
                    if t2 in self.verb_tags:
                        antecedent = self._find_antecedent(tagged, i)
                        if t2 in {'VBD', 'VBP', 'VBZ'} and w2.lower() in self.copula_verbs:
                            if j + 1 < len(tagged) and tagged[j + 1][1] == 'VBN':
                                part_word = tagged[j + 1][0]
                                by_phrase = self._find_by_phrase(tagged, j + 1)
                                if by_phrase:
                                    antecedent = self._find_antecedent(tagged, i)
                                    if antecedent:
                                        predicate = f"is {part_word} by"
                                        confidence = self._compute_confidence(antecedent, predicate, by_phrase, 'RELATIVE_PASSIVE')
                                        triples.append(SPOTriple(antecedent, predicate, by_phrase, confidence, sentence))
                                    continue
                        antecedent = self._find_antecedent(tagged, i)
                        obj = self._find_object(tagged, j)
                        if antecedent and obj:
                            predicate = self._normalize_predicate(w2, tagged, j)
                            confidence = self._compute_confidence(antecedent, predicate, obj, 'RELATIVE_SVO')
                            triples.append(SPOTriple(antecedent, predicate, obj, confidence, sentence))
                        break
        return triples

    def _find_antecedent(self, tagged: List[Tuple[str, str]], rel_idx: int) -> Optional[str]:
        i = rel_idx - 1
        while i >= 0 and tagged[i][0] == ',':
            i -= 1
        np_words = []
        while i >= 0 and tagged[i][1] in self.noun_tags | self.adj_tags | {'DT', 'PRP', 'PRP$', 'POS', 'CC', 'NNP', 'NNPS'}:
            np_words.insert(0, tagged[i][0])
            i -= 1
        if np_words:
            antecedent = ' '.join(np_words).strip()
            antecedent = re.sub(r'^(a|an|the)\s+', '', antecedent, flags=re.IGNORECASE)
            return antecedent if len(antecedent) > 1 else None
        return None

    def _extract_prepositional(self, tagged: List[Tuple[str, str]], sentence: str) -> List[SPOTriple]:
        triples = []
        for i, (word, tag) in enumerate(tagged):
            if tag in self.verb_tags and word.lower() not in self.copula_verbs:
                if i > 0 and tagged[i-1][0].lower() in self.copula_verbs and tag == 'VBN':
                    continue
                prep_objects = self._find_prepositional_objects(tagged, i)
                subject = self._find_subject(tagged, i)
                for prep, obj in prep_objects:
                    if subject and obj:
                        predicate = f"{word} {prep}"
                        confidence = self._compute_confidence(subject, predicate, obj, 'PREP')
                        triples.append(SPOTriple(subject, predicate, obj, confidence, sentence))
        return triples

    def _find_prepositional_objects(self, tagged: List[Tuple[str, str]], verb_idx: int) -> List[Tuple[str, str]]:
        results = []
        i = verb_idx + 1
        while i < len(tagged):
            if tagged[i][1] == 'IN':
                prep = tagged[i][0]
                j = i + 1
                obj_words = []
                while j < len(tagged) and tagged[j][1] in self.noun_tags | self.adj_tags | {'DT', 'PRP', 'PRP$', 'POS', 'CC', 'CD'}:
                    obj_words.append(tagged[j][0])
                    j += 1
                if obj_words:
                    obj = ' '.join(obj_words).strip()
                    obj = re.sub(r'^(a|an|the)\s+', '', obj, flags=re.IGNORECASE)
                    if len(obj) > 1:
                        results.append((prep, obj))
                i = j
            else:
                i += 1
        return results

    def _extract_apposition(self, tagged: List[Tuple[str, str]], sentence: str) -> List[SPOTriple]:
        triples = []
        for i in range(2, len(tagged) - 1):
            if tagged[i][0] == ',' and tagged[i+1][0].lower() in ('a', 'an', 'the'):
                j = i + 2
                np_words = []
                while j < len(tagged) and tagged[j][1] in self.noun_tags | self.adj_tags:
                    np_words.append(tagged[j][0])
                    j += 1
                if np_words:
                    entity = ' '.join(np_words)
                    k = i - 1
                    subject_words = []
                    while k >= 0 and tagged[k][1] in self.noun_tags | self.adj_tags | {'PRP', 'PRP$'}:
                        subject_words.insert(0, tagged[k][0])
                        k -= 1
                    if subject_words:
                        subject = ' '.join(subject_words)
                        predicate = 'is a'
                        confidence = self._compute_confidence(subject, predicate, entity, 'APPOSITION')
                        triples.append(SPOTriple(subject, predicate, entity, confidence, sentence))
                        participle_triples = self._extract_participles_in_apposition(tagged, j, subject, sentence)
                        triples.extend(participle_triples)
        # Possessive
        for i in range(1, len(tagged)):
            if tagged[i][1] == 'POS' and i > 0:
                possessor = tagged[i-1][0]
                j = i + 1
                possessed = []
                while j < len(tagged) and tagged[j][1] in self.noun_tags | self.adj_tags | {'DT'}:
                    possessed.append(tagged[j][0])
                    j += 1
                if possessed:
                    obj = ' '.join(possessed)
                    predicate = 'has'
                    confidence = self._compute_confidence(possessor, predicate, obj, 'POSSESSIVE')
                    triples.append(SPOTriple(possessor, predicate, obj, confidence, sentence))
        return triples

    def _extract_participles_in_apposition(self, tagged: List[Tuple[str, str]], start_idx: int, subject: str, sentence: str) -> List[SPOTriple]:
        triples = []
        j = start_idx
        while j < len(tagged):
            if tagged[j][1] == 'VBN':
                by_phrase = self._find_by_phrase(tagged, j)
                if by_phrase:
                    predicate = f"is {tagged[j][0]} by"
                    confidence = self._compute_confidence(subject, predicate, by_phrase, 'APPOSITION_PARTICIPLE')
                    triples.append(SPOTriple(subject, predicate, by_phrase, confidence, sentence))
                else:
                    obj = self._find_object(tagged, j)
                    if obj:
                        predicate = tagged[j][0]
                        confidence = self._compute_confidence(subject, predicate, obj, 'APPOSITION_PARTICIPLE')
                        triples.append(SPOTriple(subject, predicate, obj, confidence, sentence))
            j += 1
            if j < len(tagged) and (tagged[j][0] == ',' or (tagged[j][1] in self.verb_tags and tagged[j][0].lower() not in self.copula_verbs)):
                break
        return triples

    def _extract_apposition_with_verb(self, tagged: List[Tuple[str, str]], sentence: str) -> List[SPOTriple]:
        triples = []
        for i in range(1, len(tagged) - 2):
            if tagged[i][0] == ',':
                subject = self._find_antecedent(tagged, i)
                if not subject:
                    continue
                j = i + 1
                if j < len(tagged) and tagged[j][0].lower() in ('a', 'an', 'the'):
                    j += 1
                app_words = []
                while j < len(tagged) and tagged[j][1] in self.noun_tags | self.adj_tags | {'DT', 'CC'}:
                    app_words.append(tagged[j][0])
                    j += 1
                if not app_words:
                    continue
                while j < len(tagged):
                    if tagged[j][1] in {'VBN', 'VBG'}:
                        if tagged[j][1] == 'VBN':
                            while j < len(tagged) and tagged[j][0].lower() != ',':
                                j += 1
                        else:
                            j += 1
                    elif tagged[j][0] == ',':
                        j += 1
                    elif tagged[j][1] in self.verb_tags and tagged[j][0].lower() not in self.copula_verbs:
                        verb = tagged[j][0]
                        obj = self._find_object(tagged, j)
                        if obj:
                            predicate = self._normalize_predicate(verb, tagged, j)
                            confidence = self._compute_confidence(subject, predicate, obj, 'APPOSITION_SVO')
                            triples.append(SPOTriple(subject, predicate, obj, confidence, sentence))
                            break
                    else:
                        j += 1
        return triples

    def _propagate_subject_to_participles(self, tagged: List[Tuple[str, str]], sentence: str, svc_triples: List[SPOTriple]) -> List[SPOTriple]:
        triples = []
        for svc in svc_triples:
            if svc.predicate == 'is':
                subject = svc.subject
                complement = svc.object
                comp_tokens = complement.split()
                for i in range(len(tagged) - len(comp_tokens) + 1):
                    match = True
                    for j, ct in enumerate(comp_tokens):
                        if tagged[i + j][0] != ct:
                            match = False
                            break
                    if match:
                        k = i + len(comp_tokens)
                        while k < len(tagged) and tagged[k][1] in self.adj_tags | {'DT'}:
                            k += 1
                        if k < len(tagged) and tagged[k][1] == 'VBN':
                            verb = tagged[k][0]
                            by_phrase = self._find_by_phrase(tagged, k)
                            if by_phrase:
                                predicate = f"is {verb} by"
                                confidence = self._compute_confidence(subject, predicate, by_phrase, 'PARTICIPLE')
                                triples.append(SPOTriple(subject, predicate, by_phrase, confidence, sentence))
                            else:
                                obj = self._find_object(tagged, k)
                                if obj:
                                    predicate = verb
                                    confidence = self._compute_confidence(subject, predicate, obj, 'PARTICIPLE')
                                    triples.append(SPOTriple(subject, predicate, obj, confidence, sentence))
                        break
        return triples

    def _find_subject(self, tagged: List[Tuple[str, str]], verb_idx: int) -> Optional[str]:
        i = verb_idx - 1
        subject_words = []
        while i >= 0 and tagged[i][1] in {'MD', 'VB', 'VBD', 'VBG', 'VBN', 'VBP', 'VBZ'}:
            i -= 1
        while i >= 0 and tagged[i][0] == ',':
            i -= 1
        temp_i = i
        while temp_i >= 0:
            word, tag = tagged[temp_i]
            # Check for relative pronoun FIRST
            if word.lower() in {'which', 'that', 'who', 'whom', 'whose'}:
                break
            # Continue scanning past noun phrases, prepositions, verbs, auxiliaries, relative pronouns, adverbs, AND commas
            if tag in self.noun_tags | self.adj_tags | {'DT', 'PRP', 'PRP$', 'POS', 'CC', 'CD', 'IN', 'TO', 'MD', 'VB', 'VBD', 'VBG', 'VBN', 'VBP', 'VBZ', 'WDT', 'WP', 'WP$', 'WRB', 'RB', 'RBR', 'RBS'} or word == ',':
                temp_i -= 1
            else:
                break
        if temp_i >= 0 and tagged[temp_i][0].lower() in {'which', 'that', 'who', 'whom', 'whose'}:
            rel_start = temp_i
            while rel_start >= 0 and tagged[rel_start][0] != ',':
                rel_start -= 1
            if rel_start >= 0:
                i = rel_start - 1
                while i >= 0 and tagged[i][0] == ',':
                    i -= 1
        while i >= 0 and tagged[i][1] in self.noun_tags | self.adj_tags | {'DT', 'PRP', 'PRP$', 'POS', 'CC'}:
            subject_words.insert(0, tagged[i][0])
            i -= 1
        if subject_words:
            subj = ' '.join(subject_words).strip()
            subj = re.sub(r'^(a|an|the)\s+', '', subj, flags=re.IGNORECASE)
            return subj if len(subj) > 1 else None
        return None

    def _find_object(self, tagged: List[Tuple[str, str]], verb_idx: int) -> Optional[str]:
        i = verb_idx + 1
        object_words = []
        found_noun = False
        while i < len(tagged):
            word, tag = tagged[i]
            if tag in self.noun_tags | self.adj_tags | {'DT', 'PRP', 'PRP$', 'POS', 'CC', 'CD'}:
                object_words.append(word)
                if tag in self.noun_tags:
                    found_noun = True
            elif tag in {'IN', 'TO'} and found_noun:
                break
            elif found_noun and tag not in {'IN', 'TO', 'CC'}:
                if tag in self.adj_tags | {'DT', 'PRP', 'PRP$', 'POS', 'CD'}:
                    object_words.append(word)
                else:
                    break
            elif found_noun:
                break
            i += 1
        if object_words and found_noun:
            obj = ' '.join(object_words).strip()
            obj = re.sub(r'^(a|an|the)\s+', '', obj, flags=re.IGNORECASE)
            return obj if len(obj) > 1 else None
        return None

    def _find_complement(self, tagged: List[Tuple[str, str]], verb_idx: int) -> Optional[str]:
        i = verb_idx + 1
        comp_words = []
        found_content = False
        while i < len(tagged):
            word, tag = tagged[i]
            if tag in self.noun_tags | self.adj_tags | {'DT', 'PRP', 'PRP$', 'RB', 'IN'}:
                comp_words.append(word)
                if tag in self.noun_tags | self.adj_tags:
                    found_content = True
            elif found_content:
                break
            i += 1
        if comp_words and found_content:
            comp = ' '.join(comp_words).strip()
            comp = re.sub(r'^(a|an|the)\s+', '', comp, flags=re.IGNORECASE)
            return comp if len(comp) > 1 else None
        return None

    def _find_by_phrase(self, tagged: List[Tuple[str, str]], verb_idx: int) -> Optional[str]:
        for i in range(verb_idx + 1, len(tagged)):
            if tagged[i][0].lower() == 'by':
                j = i + 1
                by_words = []
                while j < len(tagged) and tagged[j][1] in self.noun_tags | self.adj_tags | {'DT', 'PRP', 'PRP$', 'POS', 'CC'}:
                    by_words.append(tagged[j][0])
                    j += 1
                if by_words:
                    by_phrase = ' '.join(by_words).strip()
                    by_phrase = re.sub(r'^(a|an|the)\s+', '', by_phrase, flags=re.IGNORECASE)
                    return by_phrase if len(by_phrase) > 1 else None
        return None

    def _normalize_predicate(self, verb: str, tagged: List[Tuple[str, str]], verb_idx: int) -> str:
        aux = []
        i = verb_idx - 1
        while i >= 0 and tagged[i][1] in {'MD', 'VB', 'VBD', 'VBG', 'VBN', 'VBP', 'VBZ'}:
            aux.insert(0, tagged[i][0])
            i -= 1
        if aux:
            return ' '.join(aux + [verb])
        return verb

    def _compute_confidence(self, subject: str, predicate: str, obj: str, pattern_type: str) -> float:
        pattern_weights = {
            'SVO': 0.9, 'SVC': 0.85, 'PASSIVE': 0.8, 'PREP': 0.75,
            'APPOSITION': 0.7, 'APPOSITION_SVO': 0.95, 'APPOSITION_PARTICIPLE': 0.85,
            'POSSESSIVE': 0.7, 'PARTICIPLE': 0.85, 'RELATIVE_SVO': 0.8,
            'RELATIVE_PASSIVE': 0.85,
        }
        confidence = pattern_weights.get(pattern_type, 0.5)
        if subject[0].isupper() and len(subject) > 2 and subject.lower() not in self.stopwords:
            confidence += 0.05
        if obj[0].isupper() and len(obj) > 2 and obj.lower() not in self.stopwords:
            confidence += 0.05
        if len(subject) < 3 or len(obj) < 3:
            confidence -= 0.1
        if len(subject) > 50 or len(obj) > 50:
            confidence -= 0.1
        if subject.lower() in self.stopwords:
            confidence -= 0.2
        if obj.lower() in self.stopwords:
            confidence -= 0.2
        generic_subjects = {'company', 'organization', 'group', 'team', 'system', 'model', 'technology', 'technology company'}
        if subject.lower() in generic_subjects:
            confidence -= 0.15
        return max(0.1, min(1.0, confidence))

    def _minimize_triples(self, triples: List[SPOTriple]) -> List[SPOTriple]:
        if not triples:
            return []
        triples = [t for t in triples if t.confidence >= 0.5]
        seen = set()
        unique = []
        for t in triples:
            key = (t.subject.lower().strip(), t.predicate.lower().strip(), t.object.lower().strip())
            if key not in seen:
                seen.add(key)
                unique.append(t)
        by_pred_obj = {}
        generic_subjects = {'company', 'organization', 'group', 'team', 'system', 'model', 'technology', 'technology company'}
        for t in unique:
            key = (t.predicate.lower().strip(), t.object.lower().strip())
            if key not in by_pred_obj:
                by_pred_obj[key] = t
            else:
                existing = by_pred_obj[key]
                t_is_proper = (t.subject[0].isupper() and t.subject.lower() not in self.stopwords and t.subject.lower() not in generic_subjects)
                e_is_proper = (existing.subject[0].isupper() and existing.subject.lower() not in self.stopwords and existing.subject.lower() not in generic_subjects)
                if t_is_proper and not e_is_proper:
                    by_pred_obj[key] = t
                elif t_is_proper == e_is_proper:
                    if t.confidence > existing.confidence:
                        by_pred_obj[key] = t
                    elif t.confidence == existing.confidence:
                        try:
                            t_pos = t.source_sentence.lower().index(t.subject.lower())
                            e_pos = existing.source_sentence.lower().index(existing.subject.lower())
                            if t_pos < e_pos:
                                by_pred_obj[key] = t
                        except ValueError:
                            pass
        by_pair = {}
        for t in by_pred_obj.values():
            key = (t.subject.lower().strip(), t.object.lower().strip())
            if key not in by_pair or t.confidence > by_pair[key].confidence:
                by_pair[key] = t
        minimized = list(by_pair.values())
        minimized.sort(key=lambda x: x.confidence, reverse=True)
        final = []
        for t in minimized:
            is_redundant = False
            for other in final:
                if self._is_subsumed(t, other):
                    is_redundant = True
                    break
            if not is_redundant:
                final.append(t)
        return final[:30]

    def _is_subsumed(self, t1: SPOTriple, t2: SPOTriple) -> bool:
        if t1.subject.lower() == t2.subject.lower() and t1.object.lower() == t2.object.lower():
            if t2.predicate.lower().startswith(t1.predicate.lower()):
                return True
        return False

    def _triples_to_qa(self, triples: List[SPOTriple]) -> List[Tuple[str, str]]:
        qa_pairs = []
        seen_questions = set()
        for t in triples[:15]:
            pred_lower = t.predicate.lower()
            subj = t.subject.strip()
            obj = t.object.strip()
            if subj.lower() in self.stopwords or obj.lower() in self.stopwords or len(subj) < 2 or len(obj) < 2:
                continue
            if pred_lower in ('is', 'is a', 'are', 'are a'):
                q, a = f"What is {subj}?", f"{subj} is {obj}."
            elif pred_lower.startswith('is ') and pred_lower.endswith(' by'):
                verb = pred_lower[3:-3].strip()
                q, a = f"Who {verb} {subj}?", f"{subj} {t.predicate} {obj}."
            elif pred_lower == 'has':
                q, a = f"What does {subj} have?", f"{subj} has {obj}."
            elif any(p in pred_lower for p in [' in ', ' at ', ' on ', ' from ', ' to ']):
                if ' in ' in pred_lower or pred_lower.endswith(' in'):
                    q = f"Where is {subj} located?"
                elif ' at ' in pred_lower or pred_lower.endswith(' at'):
                    q = f"Where is {subj} at?"
                else:
                    q = f"What is the relation between {subj} and {obj}?"
                a = f"{subj} {t.predicate} {obj}."
            elif pred_lower in ('develops', 'creates', 'makes', 'produces', 'builds'):
                q, a = f"What does {subj} {pred_lower}?", f"{subj} {pred_lower} {obj}."
            elif pred_lower in ('founded', 'founded by', 'established', 'created by'):
                q = f"Who {pred_lower.replace(' by', '')} {subj}?"
                a = f"{subj} {t.predicate} {obj}."
            else:
                q = f"What {t.predicate} {subj}?"
                a = f"{subj} {t.predicate} {obj}."
            q_key = q.lower().strip()
            if q_key not in seen_questions:
                seen_questions.add(q_key)
                qa_pairs.append((q, a))
        return qa_pairs[:10]

    def _generate_summary(self, triples: List[SPOTriple], key_facts: List[str], topic_hint: str) -> str:
        if not triples and not key_facts:
            return topic_hint or "No information extracted."
        if triples:
            top_triples = triples[:3]
            parts = [f"{t.subject} {t.predicate} {t.object}" for t in top_triples]
            return '. '.join(parts) + '.'
        return key_facts[0][:300] if key_facts else topic_hint

    def _extract_topic(self, text: str, triples: List[SPOTriple]) -> str:
        if triples:
            subjects = [t.subject for t in triples]
            from collections import Counter
            subj_counts = Counter(s.lower() for s in subjects if s.lower() not in self.stopwords)
            if subj_counts:
                most_common = subj_counts.most_common(1)[0][0]
                for s in subjects:
                    if s.lower() == most_common:
                        return s
        words = word_tokenize(text[:500])
        tagged = pos_tag(words)
        for word, tag in tagged:
            if tag in {'NNP', 'NNPS'} and len(word) > 2:
                return word
        return "Unknown Topic"


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage: openie_extractor.py <text> [topic_hint]"}))
        sys.exit(1)
    text = sys.argv[1]
    topic_hint = sys.argv[2] if len(sys.argv) > 2 else ""
    extractor = OpenIEExtractor()
    result = extractor.extract(text, topic_hint)
    output = {
        "topic": result.topic, "summary": result.summary, "key_facts": result.key_facts,
        "qa_pairs": [{"question": q, "answer": a} for q, a in result.qa_pairs],
        "entities": result.entities,
        "spo_triples": [{"subject": t.subject, "predicate": t.predicate, "object": t.object, "confidence": t.confidence, "source_sentence": t.source_sentence} for t in result.spo_triples],
        "json_ld": result.json_ld
    }
    print(json.dumps(output, ensure_ascii=False))


if __name__ == "__main__":
    main()