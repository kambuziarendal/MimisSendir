#ifndef SOUND_SYSTEM_H
#define SOUND_SYSTEM_H

// TODO: should these be worded in a more generic way so they can be shared with other chat apps?
// Or is it useful to have different sounds for each app? The system sounds preferences won't
// scale well if every app comes with a handful of sound events.
#define kSoundNewMessage "MímisSendir new message"
#define kSoundNewMUCMessage "MímisSendir new groupchat message"
#define kSoundUserOnline "MímisSendir user online"
#define kSoundUserOffline "MímisSendir user offline"

// Independent mention sound, without changing another application's event.
#define kSoundHighlight "MímisSendir mention"

#endif /* SOUND_SYSTEM_H */
