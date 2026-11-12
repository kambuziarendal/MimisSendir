//////////////////////////////////////////////////
// Blabber [Messages.h]
//     BMessage identities for GUI communication.
//////////////////////////////////////////////////

#ifndef MESSAGES_H
#define MESSAGES_H

// UI messages
#define kRosterSelectionChanged			'rstc'

// BLABBER MESSAGES
#define BLAB_UPDATE_ROSTER              0xA0000000

// generic
#define JAB_OK                          0x12000000
#define JAB_CANCEL                      0x12000FFF

// GUI
#define JAB_CLOSE                       0x13000000
#define JAB_ABOUT                       0x13003333
#define JAB_QUIT                        0x13006666

#define JAB_PREFERENCES                 0x1300AAAA
#define JAB_SETTINGS_SAVED              'sSvd'
#define JAB_JABBER_ORG                  0x1300DDDD
#define JAB_JABBER_VIEW_COM             0x1300DDDF
#define JAB_USER_GUIDE                  0x1300EEEE
#define JAB_RIV                         0x1300EEEF
#define JAB_FAQ                         0x1300EEF0
#define JAB_BE_USERS                    0x1300EEF1

// communication
#define JAB_OPEN_MESSAGE                0x14000000
#define JAB_CLOSE_MESSAGE               0x14001000

#define JAB_OPEN_CHAT                   0x14001888
#define JAB_OPEN_NEW_CHAT               0x14002000
#define JAB_CLOSE_CHAT                  0x14002888

#define JAB_OPEN_NEW_GROUP_CHAT         0x14002900
#define JAB_CLOSE_GROUP_CHAT            0x14002904

#define JAB_MESSAGE_SENT                0x14004000
#define JAB_MESSAGE_RECV                0x14004888

#define JAB_CHAT_SENT                   0x14005000
#define JAB_CHAT_RECV                   0x14005888

#define JAB_GROUP_CHATTER_ONLINE        0x14006000
#define JAB_GROUP_CHATTER_OFFLINE       0x14006888

#define NEWLINE_TOGGLE                  0x1400B999

#define JAB_START_RECORD                0x1400C000
#define JAB_STOP_RECORD                 0x1400C888
#define JAB_SHOW_CHATLOG                0x1400C001

#define JAB_CLOSE_TALKS                 0x1400C999

#define JAB_ROTATE_CHAT_FORWARD         0x1400D000
#define JAB_ROTATE_CHAT_BACKWARD        0x1400D001
#define JAB_FOCUS_BUDDY                 0x1400D002

// roster
#define JAB_OPEN_ADD_BUDDY_WINDOW       0x15000000
#define JAB_ADD_NEW_BUDDY               0x15001000
#define JAB_ADD_BUDDY_FROM_ROSTER       0x15002000
#define JAB_OPEN_EDIT_BUDDY_WINDOW      0x15003000
#define JAB_EDIT_BUDDY                  0x15004000
#define JAB_REMOVE_BUDDY                0x15005000
#define JAB_GOT_ROSTER_FROM_SERVER      0x15006000
#define JAB_USER_INFO                   0x15007000

// jabber
#define JAB_RECONNECTING                0x16000002
#define JAB_RESET                       0x16001000
#define JAB_DISCONNECT                  0x16002000
#define JAB_DISCONNECTED                0x16003000
#define JAB_ABNORMAL_DISCONNECT         0x16004000

#define JAB_LOGIN                       0x16006000
#define JAB_LOGGED_IN                   0x16007000
#define JAB_LOGOUT                      0x16008000

// presence
#define JAB_SUBSCRIBE_PRESENCE          0x17000000
#define JAB_UNSUBSCRIBE_PRESENCE        0x17002000

#define BLAB_ONLINE                     0x17002FFF
#define BLAB_AVAILABLE_FOR_CHAT         0x17003000
#define BLAB_DO_NOT_DISTURB             0x17003001
#define BLAB_AWAY_TEMPORARILY           0x17003002
#define BLAB_AWAY_EXTENDED              0x17003003
#define BLAB_CUSTOM_STATUS              0x17003004

#define BLAB_SCHOOL                     0x17003005
#define BLAB_WORK                       0x17003006
#define BLAB_LUNCH                      0x17003007
#define BLAB_DINNER                     0x17003008
#define BLAB_SLEEP                      0x17003009

// transports
#define AGENT_MENU_CHANGED_TO_IRC       0x18000003
#define AGENT_MENU_CHANGED_TO_JABBER    0x18000004
#define REGISTER_TRANSPORT              0x18000005
#define UNREGISTER_TRANSPORT            0x18000006
#define TRANSPORT_UPDATE                0x18000007

// sounds
#define JAB_SELECTED_NEW_MESSAGE_SOUND  0x19000000
#define JAB_NO_NEW_MESSAGE_SOUND        0x19000001
#define NEW_MESSAGE_FILE_OPEN           0x19000003

#define JAB_SELECTED_MESSAGE_SOUND      0x19000004
#define JAB_NO_MESSAGE_SOUND            0x19000005
#define MESSAGE_FILE_OPEN               0x19000007

#define JAB_SELECTED_USER_ONLINE_SOUND  0x19000008
#define JAB_NO_USER_ONLINE_SOUND        0x19000009
#define USER_ONLINE_FILE_OPEN           0x1900000B

#define JAB_SELECTED_ALERT_SOUND        0x1900000C
#define JAB_NO_ALERT_SOUND              0x1900000D

#define kOptionChanged                  0x1C000001
#define kEditTopic                      0x1C000002

#define JAB_SELECTED_USER_OFFLINE_SOUND 0x1C000008
#define JAB_NO_USER_OFFLINE_SOUND       0x1C000009
#define USER_OFFLINE_FILE_OPEN          0x1C00000B

#endif
