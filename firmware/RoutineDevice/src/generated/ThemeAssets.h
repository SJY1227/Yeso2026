#pragma once
#include "Assets.h"
namespace assets {
struct CharacterTheme {
  const CompressedImage *topbar, *arrows, *background, *homeBase, *paper;
  uint16_t panel, button, accent, track, greeting, routineButton;
};
extern const CharacterTheme characterThemes[10];
extern const CompressedImage* homeCharacterArt[30];
extern const CompressedImage* rewardFoodArt[31];
extern const CompressedImage letterNotification;
const CharacterTheme& characterTheme(uint8_t id);
}
