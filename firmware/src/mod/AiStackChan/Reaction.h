#ifndef _REACTION_H
#define _REACTION_H

#include <Arduino.h>
#include <Avatar.h>

// Data-driven reaction sequences: a reaction is an array of steps, each step
// sets an expression / balloon text / servo pose / tone, then holds. Adding a
// new reaction only requires adding data to REACTIONS[] in Reaction.cpp.

struct ReactionStep {
  m5avatar::Expression expr;
  const char* balloon;   // balloon text ("" = clear, nullptr = leave as-is)
  int16_t servoX;        // pan offset in degrees from the servo origin
  int16_t servoY;        // tilt offset in degrees from the servo origin (negative = tilt back)
  uint16_t servoMs;      // move duration; 0 = no servo move in this step
  uint16_t toneHz;       // 0 = no tone
  uint16_t toneMs;
  uint16_t holdMs;       // wait before next step
};

struct Reaction {
  const char* name;
  const char* const* keywords;   // nullptr-terminated STT substrings
  const ReactionStep* steps;
  size_t nSteps;
};

// Returns the first reaction whose keyword appears in sttText, else nullptr.
const Reaction* matchReaction(const String& sttText);

// Looks up a reaction by its name ("bang", "dart", "blow_kiss", "wave",
// "heart"...). Used by the vision gesture classifier.
const Reaction* findReactionByName(const char* name);

// Plays a reaction synchronously (loop context). Pauses idle servo motion,
// restores Neutral + servo origin at the end.
void playReaction(const Reaction& r);

#endif  //_REACTION_H
