/*
 * Copyright (C) 2009, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LatinIME: dictionary.cpp"

#include "suggest/core/dictionary/dictionary.h"

#include <map> // TODO: remove
#include <stdint.h>

#include "defines.h"
#include "suggest/core/dictionary/bigram_dictionary.h"
#include "suggest/core/dictionary/binary_format.h"
#include "suggest/core/session/dic_traverse_session.h"
#include "suggest/core/suggest.h"
#include "suggest/core/suggest_options.h"
#include "suggest/policyimpl/gesture/gesture_suggest_policy_factory.h"
#include "suggest/policyimpl/typing/typing_suggest_policy_factory.h"

namespace latinime {

Dictionary::Dictionary(void *dict, int dictSize, int mmapFd, int dictBufAdjust)
        : mBinaryDicitonaryInfo(static_cast<const uint8_t *>(dict), dictSize),
          mDictSize(dictSize),
          mDictFlags(BinaryFormat::getFlags(mBinaryDicitonaryInfo.getDictBuf(), dictSize)),
          mMmapFd(mmapFd), mDictBufAdjust(dictBufAdjust),
          mBigramDictionary(new BigramDictionary(&mBinaryDicitonaryInfo)),
          mGestureSuggest(new Suggest(GestureSuggestPolicyFactory::getGestureSuggestPolicy())),
          mTypingSuggest(new Suggest(TypingSuggestPolicyFactory::getTypingSuggestPolicy())) {
}

Dictionary::~Dictionary() {
    delete mBigramDictionary;
    delete mGestureSuggest;
    delete mTypingSuggest;
}

int Dictionary::getSuggestions(ProximityInfo *proximityInfo, DicTraverseSession *traverseSession,
        int *xcoordinates, int *ycoordinates, int *times, int *pointerIds, int *inputCodePoints,
        int inputSize, int *prevWordCodePoints, int prevWordLength, int commitPoint,
        const SuggestOptions *const suggestOptions, int *outWords, int *frequencies,
        int *spaceIndices, int *outputTypes) const {
    int result = 0;
    if (suggestOptions->isGesture()) {
        DicTraverseSession::initSessionInstance(
                traverseSession, this, prevWordCodePoints, prevWordLength, suggestOptions);
        result = mGestureSuggest->getSuggestions(proximityInfo, traverseSession, xcoordinates,
                ycoordinates, times, pointerIds, inputCodePoints, inputSize, commitPoint, outWords,
                frequencies, spaceIndices, outputTypes);
        if (DEBUG_DICT) {
            DUMP_RESULT(outWords, frequencies);
        }
        return result;
    } else {
        DicTraverseSession::initSessionInstance(
                traverseSession, this, prevWordCodePoints, prevWordLength, suggestOptions);
        result = mTypingSuggest->getSuggestions(proximityInfo, traverseSession, xcoordinates,
                ycoordinates, times, pointerIds, inputCodePoints, inputSize, commitPoint,
                outWords, frequencies, spaceIndices, outputTypes);
        if (DEBUG_DICT) {
            DUMP_RESULT(outWords, frequencies);
        }
        return result;
    }
}

int Dictionary::getBigrams(const int *word, int length, int *inputCodePoints, int inputSize,
        int *outWords, int *frequencies, int *outputTypes) const {
    if (length <= 0) return 0;
    return mBigramDictionary->getBigrams(word, length, inputCodePoints, inputSize, outWords,
            frequencies, outputTypes);
}

int Dictionary::getProbability(const int *word, int length) const {
    const uint8_t *const root = mBinaryDicitonaryInfo.getDictRoot();
    int pos = BinaryFormat::getTerminalPosition(root, word, length,
            false /* forceLowerCaseSearch */);
    if (NOT_VALID_WORD == pos) {
        return NOT_A_PROBABILITY;
    }
    const uint8_t flags = BinaryFormat::getFlagsAndForwardPointer(root, &pos);
    if (flags & (BinaryFormat::FLAG_IS_BLACKLISTED | BinaryFormat::FLAG_IS_NOT_A_WORD)) {
        // If this is not a word, or if it's a blacklisted entry, it should behave as
        // having no probability outside of the suggestion process (where it should be used
        // for shortcuts).
        return NOT_A_PROBABILITY;
    }
    const bool hasMultipleChars = (0 != (BinaryFormat::FLAG_HAS_MULTIPLE_CHARS & flags));
    if (hasMultipleChars) {
        pos = BinaryFormat::skipOtherCharacters(root, pos);
    } else {
        BinaryFormat::getCodePointAndForwardPointer(root, &pos);
    }
    const int unigramProbability = BinaryFormat::readProbabilityWithoutMovingPointer(root, pos);
    return unigramProbability;
}

bool Dictionary::isValidBigram(const int *word1, int length1, const int *word2, int length2) const {
    return mBigramDictionary->isValidBigram(word1, length1, word2, length2);
}

int Dictionary::getDictFlags() const {
    return mDictFlags;
}

} // namespace latinime