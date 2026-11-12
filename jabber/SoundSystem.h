#ifndef SOUND_SYSTEM_H
#define SOUND_SYSTEM_H

// TODO: should these be worded in a more generic way so they can be shared with other chat apps?
// Or is it useful to have different sounds for each app? The system sounds preferences won't
// scale well if every app comes with a handful of sound events.
#define kSoundNewMessage "Renga new message"
#define kSoundNewMUCMessage "Renga new groupchat message"
#define kSoundUserOnline "Renga user online"
#define kSoundUserOffline "Renga user offline"

// Reuse existing setting from Vision for now
#define kSoundHighlight "Vision Nick Notification"

#endif /* SOUND_SYSTEM_H */
