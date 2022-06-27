# 1.6.2 - 2022-06-26
- The text-mode client should now support Unicode input when in UTF-8
  locales, e.g. allowing player names containing accented characters
  to be input (#60).
- Add support for networking on the Haiku operating system (thanks
  to Begasus) (#61).

# 1.6.1 - 2020-12-11
- Improved display in non-English text-mode clients; previously
  columns were not aligned properly in some cases and occasionally
  not all available drugs at a location were shown on screen (#54).
- Minimal British English translation added (#57). Configuration file
  entries can now use either British English or American English spelling
  (i.e. Armor/Armour).
- On Windows the "browse" button in the graphical client options dialog
  now opens at the folder containing the selected sound file (#55).

# 1.6.0 - 2020-12-06
- Fixes to build with OpenWRT (thanks to Theodor-Iulian Ciobanu).
- Write server pidfile after fork (thanks to Theodor-Iulian Ciobanu).
- Updated German and French Canadian translations from Benjamin Karaca
  and Francois Marier.
- Support for old GTK1 and GLIB1 libraries removed - we now need version 2
  of these libraries to build dopewars. GTK+3 is also supported.
- Update metaserver to work with new SourceForge; older versions can no
  longer successfully register with the metaserver.
- Switch to using libcurl to talk to the metaserver (this supports https,
  unlike the old internal code). The metaserver configuration has changed
  accordingly; `MetaServer.Name`, `MetaServer.Port` and `MetaServer.Path` are
  replaced with `MetaServer.URL`, while `MetaServer.Auth`, `MetaServer.Proxy*`,
  and `MetaServer.UseSocks` are removed (set the `https_proxy` environment
  variable instead, as per
  https://curl.haxx.se/libcurl/c/libcurl-tutorial.html#Environment)
- The default web browser on Linux has changed from 'mozilla' to
  'firefox'; on Mac the system-configured default browser is used.
- On Windows the high score file, log file, and config file are now
  stored in the user application data directory (e.g.
  `C:\Users\foo\AppData\Local\dopewars`) rather than the same directory as
  the dopewars binary.
- Add sound support with SDL2, and on Mac.
- Add 64-bit Windows binaries.
- Fix for a DOS against the server using the REQUESTJET message type
  (thanks to Doug Prostko for reporting the problem).

# 1.5.12 - 2005-12-30
- Really fix a potential exploit against the Win32 server when running as
  an NT service (user data was being used as a format string in some cases).

# 1.5.11 - 2005-12-30
- Add example configuration file to the documentation.
- Fixed various typos in the German translation (thanks to Jens Seidel
  and Francois Marier).
- Fix a remote exploit against the Win32 server (thanks to KF).
- High score file on Windows is now written into local application data
  directory if available, to work more efficiently on multi-user systems.

# 1.5.10 - 2004-10-24
- High score file is now installed in `${localstatedir}` rather than
  `${datadir}`, to allow proper Filesystem Hierarchy Standard compliance
- Fix for a curses client crash if the D key is pressed during attacks
  by the cops
- Some problems with the curses client missing screen resize events fixed
- Logging to a file should now work properly again
- Minimum and maximum limits on all relevant integer configuration
  file variables are now imposed for sanity
- Quique's Spanish translation is now available both in standard Spanish
  (`es.po`) and `es_ES.po`, which uses drugs slang from Spain
- Fix for a trivial DOS of the server
- Windows installer no longer hardcodes `C:\Program Files` so should
  work with non-English versions of Windows

# 1.5.9 - 2003-06-07
- The messages window in the curses client can now be scrolled with the
  + and - keys
- The curses client now makes better use of space with screen sizes
  larger than 80x24
- Fix for a crash encountered if you drop drugs and then encounter the cops
- Addition of -P, --player command line option to set the player name
  to use (thanks to Michael Mitton)

# 1.5.8 - 2002-10-21
- Options dialog now allows sounds for all supported game events to be set
- BindAddress config variable added, to allow the server to be bound to
  a non-default IP address
- BankInterest and DebtInterest variables added, to allow the
  configuration of interest rates (with thanks to Matt)
- New "UTF8" ability added; if client and server share this ability, then
  all network messages will be sent in UTF-8 (Unicode) encoding (without
  the ability, all messages are assumed to be in your locale's default
  codeset, which may cause problems on non-US ASCII systems)
- Names.Month and Names.Year have been replaced with StartDate.Day,
  StartDate.Month, StartDate.Year and Names.Date; these can be used to
  handle the date display properly after the turn number exceeds 31
- encoding and include config directives added, to allow the config file's
  encoding (usually taken from the locale) to be overridden, and to allow
  the inclusion of other config files
- Spanish translation added by Quique
- The Windows build of dopewars should now use Unicode throughout, on
  platforms with Unicode support (i.e. NT/2000/XP)
- Under Windows XP, the "pretty" new common controls are now used
- Sounds provided by Robin Kohli of www.19.5degs.com

# 1.5.7 - 2002-06-25
- Sound support; Windows multimedia, ESD and SDL outputs are supported;
  the individual modules can be statically linked in, or built as true
  "plugins"
- Version mismatches between client and server are now treated more
  sensibly (it's all done server-side, and spurious warnings are now
  removed - only an old client connecting to a new server will
  trigger them)
- Bug fix: when the buttons in the Fight dialog are not visible to a
  mouse user, previously you were able to access them via the keyboard
  shortcuts; now fixed.
- configure should now work properly if GLib 2.0 is installed but
  GTK2.0 is not
- Norwegian Nynorsk translation added by �smund
- If dopewars is run setuid/setgid, it will now only use this privilege
  to open the default (hard-coded) high score file; it will not open
  a user-specified high score file with privilege
- It is no longer necessary to run "dopewars -C" on a zero-byte high
  score file; it will be converted automatically
- A new server command "save" can be used to save the current configuration
  to a named config file

# 1.5.6 - 2002-04-29
- Bug fix: the server will only let you pay back loans or deal with the
  bank when you are at the correct location, and you can no longer
  "pay back" negative amounts of cash to the loan shark
- Minor improvements to fighting code
- The GTK+2 client should now run properly in non-UTF8 locales, and
  handle configuration files in both UTF8 and non-UTF8 locales
- Unsafe list iteration in serverside code (which could possibly cause
  memory corruption) fixed
- Another dumb PPC bug fixed
- Incorrect LIBS generated by configure script in some circumstances
  (due to a GTK+/Glib bug) - now fixed
- Everything should now build with autoconf-2.53 (if desired)

# 1.5.5 - 2002-04-13
- On fight termination the player is now allowed to close the "Fight"
  dialog before any new dialogs pop up
- Bug caused by a "fight" interrupting a "deal" fixed
- dopewars no longer crashes if you set e.g. NumGun = 0
- Incorrect handling of `WM_CLOSE` under Win32 fixed
- Unix server now fails "gracefully" if it cannot create the Unix domain
  socket for admin connections
- New ServerMOTD variable to welcome players to a server (with thanks
  to Mike Robinson)
- GTK+ client should now work with GTK+2.0

# 1.5.4 - 2002-03-03
- Basic configuration file editor added to GTK+ client
- Annoying flashing on closure of modal windows in Win32 fixed
- Win32 client now uses "proper" dialog boxes (i.e. without a window menu)
- Icon added for GTK+ client
- Bug with withdrawing cash from the bank fixed
- URL in GTK+ client "About" box is now clickable
- Crash bugs when running on PPC systems fixed (with thanks to Zeke
  and Brian Campbell)

# 1.5.3 - 2002-02-04
- Text-mode server is now non-interactive by default (server admin can
  connect later with the -A option)
- Windows server can now be run as an NT Service
- Fatal bug when visiting the bank (under Win2000/XP) fixed
- Windows installer should now upgrade old versions properly
- Currency can now be configured with Currency.Symbol and Currency.Prefix
- Windows client windows cannot now be made unreadably small
- Bank/loan shark dialog now warns on entering negative prices
- Default configuration is restored properly at the start of each game
- Translations should now work with the Windows client
- Documentation on the client-server protocol added
- Windows graphical server can be minimized to the System Tray
- Keyboard shortcuts for menu items in Windows client
- Default buttons (ENTER -> "OK") for Windows client
- RPM build/make install can now be run as non-superuser
- Win32 install for current user/all users
- Code cleanups

# 1.5.2 - 2001-10-16
- Slightly easier-to-use "run from fight" Jet dialog (avoids the crazy
  "windows pop up faster than you can close them" syndrome)
- Support for HTTP proxies and authentication
- SOCKS4 and SOCKS5 (user/password) support
- French translation added by leonard
- Boolean configuration variables (TRUE/FALSE) now supported
- Many code cleanups
- High score files now have a "proper" header, so that file(1) can
  identify them, and so the -f option cannot be used to force setgid-games
  dopewars to overwrite random files writeable by group "games" - use
  the -C option to convert old high score files to the new format
- GNU long command line options now accepted on platforms with `getopt_long`
- Simple installer now in place for Win32 systems

# 1.5.1 - 2001-06-19
- Improved logging in server via LogLevel and LogTimestamp variables
- Metaserver (both client and server) moved to SourceForge
- Icons (courtesy of Ocelot Mantis) and GNOME desktop entry added

# 1.5.0 - 2001-05-13
- Fixes for spurious tipoffs
- High scores should now be written properly on Win32 systems
- Various minor usability fixes on Win32 systems

# 1.5.0beta2 - 2001-04-29
- Various fixes for installation on BSD systems and Mac OS X
- Multiplayer menus (spy on player, etc.) are now greyed out in GTK+ client
  when in single-player mode
- Manpage (courtesy of Leon Breedt) added in doc/
- Fix for missing "bgetch" when configured with --disable-curses-client
- Broken "trenchcoat" message fixed
- Value of bought drugs now displayed in curses client
- AI players now are at least partially functional
- Fix for server segfault on invalid short network messages
- dopewars no longer runs GTK+ setgid
- "make install" installs dopewars as group "wheel" if "games" is
  unavailable

# 1.5.0beta1 - 2001-04-08
- Completely rewritten fighting code
- Internationalization (i18n) support
- Tense and case-sensitive translated strings handled via `dpg_` analogues
  to glib's `g_` string handling functions `%P` = price,
  `%Txx` or `%txx` = tense-sensitive string, `%/.../` = comment (ignored)
- Networking revamped - now uses nonblocking sockets to improve server
  responsiveness and to remove deadlocks (previously, any client could
  halt server by sending an unterminated message); "abilities" added to
  allow backwards-compatible protocol extensions; player IDs used rather
  than player names to save bandwidth, with newer client+server
- Drug values now stored by server (e.g. "you have 5 Weed @ $600); sent
  only if DrugValue config. variable is set, and only to new clients
  (based on a patch by Pierre F)
- Spying fixed (cannot now spy on a player until they accept your bitch)
- Longer `T>alk` and `P>age` messages allowed in curses client
- Minor bug fixes to configure options
- configure script tweaked to fix networking under Solaris (and friends)
  (with thanks to Caolan McNamara)
- Client-side code moved out of `clientside.c` and `dopewars.c`;
  client-specific code now placed in `<xxx>_client.c`, while generic code is
  in `message.c`
- GTK+ client added
- Native "pointy-clicky" Win32 graphical client added (via GTK+ emulation)
- GLib dependency introduced; string and list handling is taken care of
  now by GLib routines
- Configuration files now handled by GLib's GScanner; "string lists"
  (of the format { "string1", "string2", "string3" } ) are now supported
  for configuration of subway sayings, "stopped to" and overheard songs
- Timeouts bug fixed
- MaxClients bug fixed

# 1.4.8 - 2000-07-09
- Several fixes to Win32 networking code
- IdleTimeout and ConnectTimeout variables added, to allow the server to
  break connections that have been idle for too long, or take too long
  to (dis)connect, respectively
- Servers now use UDP packets to communicate with metaserver, for 
  a faster response to changing game conditions; the client, and older
  versions of the server, still use the "old" CGI script interface;
  MetaServer.Port variable split into .HttpPort and .UdpPort
- MetaServer.Password can now be used with a blank MetaServer.LocalName
  (with the new metaserver interface only) in order to identify servers
  whose IPs are dynamic (but are otherwise the "same" server); this password
  must, again, be acquired from the metaserver maintainer
- Metaserver now records current & maximum numbers of players, high
  scores, and last update time and uptime, for each server
- Servers now re-register with metaserver when players join or leave,
  on receipt of a SIGUSR1 signal, and periodically
- Metaserver list in client now lists uptime, and current/maximum
  numbers of players
- Pid file maintained while in server mode (-r command line switch)
- Names of the gun shop, pub, bank and loan shark can now be customised
  (GunShopName, RoughPubName, BankName and LoanSharkName)
- When a player tries to run from a fight, running to the current location
  now takes them back to the fighting screen

# 1.4.7 - 2000-01-14
- Minor fixes to Win32 code
- dopewars now uses autoconf to (hopefully) build properly on odd sytems
  such as HP-UX, and also to build "out of the box" under Cygwin (win32)
- long long datatype used for all prices on platforms that support it
- fixes to strtoprice and pricetostr code; replacement of code which
  uses printf("%ld") for prices with pricetostr calls (with thanks to
  Coolio)
- "Leave" option added to Bank
- Messages window is now only displayed for network games
- Binary can be compiled without TCP/IP networking support (e.g. for use
  on standalone systems) by configuring with --disable-networking
- Minor modification to config. file handling to allow variables to be set
  to null strings (use "Variable=")
- Option to allow the "local" server name to be specified when registering
  with the metaserver - MetaServer.LocalName variable. Useful when the
  metaserver refuses to resolve your IP address to your "preferred" domain
  name or when connecting via an enforced web proxy. Email the metaserver
  maintainer, for an authentication password (MetaServer.Password) linked to
  your chosen domain name, to use this option successfully.

# 1.4.6 - 1999-11-12
- Bug fix for message window and "sew you up" prompt
- Bug fix for server hanging in LoseBitch function
- If player opts to play again, server selection method used last time
  is used again
- Terminal resizing now handled properly
- Port to Win32 (Windows 95,98,NT) console mode

# 1.4.5 - 1999-10-21
- Limited support now for terminals at sizes other than 80x24; but response
  to a resize during the program run doesn't work properly yet...
- Minor improvements to AI players
- Corrected website address displayed by client on connecting to a
  server of a different version
- If player opts to play again, defaults to the name they used last time
- Server now disconnects clients when their game ends (rather than
  waiting for them to politely disconnect) - this gets around the problem
  of particularly unresponsive clients getting killed and then sitting
  around in an "undead" state, able to be repeatedly killed by other
  players
- Armed players cannot now "stand and take it" (why would you want to
  anyway?) in multiplayer fights
- Client now offers to obtain the list of available servers from the
  metaserver, to select one to connect to
- "Special" values (MetaServer), (Prompt) and (Single) (including the
  brackets) now accepted for the "Server" variable, which instruct the
  client to list the servers, prompt the user for a server, or play
  in single-player mode, rather than connecting immediately to a server
- "MetaServer.Port" variable added to facilitate connection to the
  metaserver via a proxy server (with thanks to Tony Brown)
- Signal handling cleaned up
- Buffer overflow problem with ExtractWord() fixed (hopefully) (with thanks
  to Lamagra)
- Command line option -S for running a "private" server (do not contact
  the metaserver)
- Prices for spies and tipoffs can be customised; this information is not
  communicated properly between 1.4.5 and earlier versions, of course.
  In such a case, the game will still work properly, although the client
  may report erroneous spy and tipoff prices
- Fixed dodgy "pricetostr" function
- Bug fix for "Drop" command in single-player mode
- Command line option -g for specifying a supplementary configuration file
- FightTimeout variable fixed - it now actually does something...
- GunShop, LoanShark, RoughPub and Bank variables corrected so that they
  take actual location numbers now - not (location-1). WARNING: this
  breaks old configuration files!
- Full HTML documentation now provided
- Prices of bitches for hire can now be configured - Bitch.MinPrice and
  Bitch.MaxPrice
- Removed description of non-existent "die" command in server
- Minor fixes in antique mode
- Fix of NumDrug and NumGun processing (now allows more than the default
  number of drugs and guns) (with thanks to Matt Higgins)
- "ConfigVerbose" option added to display extra feedback during config
  file processing (with thanks to Matt Higgins)

# 1.4.4 - 1999-09-16
- Full compatibility with 1.4.3 servers and clients maintained
  (although a warning is displayed to upgrade as soon as possible)
- dopewars client now properly redraws the screen when Ctrl-L is pressed
- Server output is now line-buffered by default for more sensible output
  of log files
- `L>ist` bug in single player mode fixed
- Number of game turns can now be configured with the "NumTurns"
  variable, or the game can be left to go on forever if it's set to zero
- The shortcuts "k" and "m" are now supported in any input of numbers
  (e.g. money to put in the bank). So, for instance, typing 1.5m would
  be short for typing 1500000  (m=million, k=thousand)
- Server now automatically contacts the dopewars metaserver (actually
  a CGI script), at bellatrix.pcl.ox.ac.uk, whenever it is brought up or
  down, to keep the list of servers on the dopewars webpage up to date.
  Aspects of the server's communication with the metaserver can be
  configured with the MetaServer.xxx variables
- Names of the two police officers which chase you (originally
  Hardass and Bob) can now be configured with the variables
  "Names.Officer" and "Names.ReserveOfficer" respectively
  (provided by: Mike Meyer)
- Several uses of the  string constant "bitches" rather than
  the variable "Names.Bitches" have been spotted, and corrected
  (provided by: Mike Meyer)
- "Sanitized" variable - if nonzero, removes drug references
  (random events, the cops, etc.) - obviously drug names need to also
  be changed in the config. file to complement this. Turns dopewars into
  a simple trading game (provided by: Mike Meyer)
- Minor formatting cleanups to accommodate longer drug names on the
  screen neatly (provided by: Mike Meyer)

# 1.4.3 - 1999-06-23
- Bug with random offer of weed/paraquat fixed
- `L>ist` command now offers list of logged-on players or high scores
- "Out of time" message to explain why the game stops suddenly after 31 days
- Bank is now a little more user-friendly
- Messages announcing players leaving or joining the game now appear in
  the central "messages" window, rather than the main, bottom window
- Clients should now behave properly after the server crashes (or they
  are pushed off the server) - i.e. they should revert to a single-player
  mode game
- `price_t` type used for all prices
- Server interactive interface is now greatly improved, complete with
  help screen
- `SO_REUSEADDR` set so that server can be restarted immediately if it crashes
- Facility to drop unwanted drugs, with the accompanying chance that you
  are caught by Officer Hardass and shot at
- Fighting interface greatly improved:-
  - All player-player fighting now occurs in a specialised window. Players
    can switch between the standard "deal drugs" window and the fighting
    window with the D and F keys
  - Number of keystrokes required to shoot and acknowledge all the
    relevant messages now greatly reduced
  - Some indication is now given of the other player's status (number of
    bitches and guns)
  - Server now imposes timeouts on fights, so if an opponent does not
    return fire within a set time, a repeat attack is allowed
  - A bounty is paid out for killing an enemy bitch, and any guns/drugs
    they're carrying are passed on to the victor (if he/she is able to
    carry them)
  - A dead player's cash is appropriated by the victor of a fight
- Handling of configuration files now greatly improved; the same options
  that are set here can also be set within the server as long as no
  players are connected. A large number of dopewars settings can be
  changed and customised from here. Customised settings will be used
  in single-player mode, and if dopewars is used as a server the settings
  will be propagated to any clients (of version 1.4.3 or higher) that
  connect. Not everything can be customised, but any remaining changes
  should be server-side only (and thus require no alteration to the 
  clients). Options include:-
  - MaxClients option to limit the maximum number of players connected
    to the server
  - FightTimeout option to alter the length of the fight timeout
  - StartCash and StartDebt to change the default starting cash and debt
    of every player
  - Probabilities and toughness of Officer Hardass and his deputies can
    be "tweaked"
  - Numbers and names of locations, drugs and guns can be altered
  - The words used to denote "bitches", "guns" and "drugs" can be 
    customised
  - Drugs can now be sorted by name or by price, in forwards or reverse
    order, with the DrugSortMethod option (can take values 1-4)

# 1.4.2 - 1999-05-16
- AI player improvements
- Message structure changed to use less bandwidth and neater code
- Now easier to break out of buy/sell drug prompts etc. (by pressing an 
  'invalid' key or ENTER)
- Cleanup of player list
- Cleanup after a player leaves the server; i.e. remove any references to
  their spies or tipoffs with other players
- Added highlight of most recent score (for systems without working 
  `A_STANDOUT` attribute)
- Fixed bug which caused all street-bought (i.e. not at Dan's gun shop)
  guns to be Saturday Night Specials
- Prevented badly-behaved clients from continuing to jet to new locations
  after their death 
- Added code to remove whitespace from name=value data read from 
  configuration file, and defaulted from $HOME/.dopewars to /etc/dopewars
- Added "helpful" messages when guns cannot be bought or sold in gun shop
- Minor cleanups of player-player fighting messages

# 1.4.1b - 1999-04-28
- segfault bug in server fixed

# 1.4.1a - 1999-04-28
- Interim release before 1.4.2; a few bug fixes in antique mode

# 1.4.1 - 1999-04-27
- Fix of bug where paying off your debt would actually _increase_ it!
  Dunno how that one slipped through... I blame my beta testers... ;)

# 1.4.0 - 1999-04-27
- Fixed bug with server; server now detects if standard input has
  been closed properly (previously if its input was redirected from
  /dev/null it would keep trying to read from it, using 100% CPU. Oops.)
- First release under GPL

# 1.3.8 - 1999-04-26
- Message structure changed; separator changed from : to ^ and extra
  field added to identify messages to AI players
- Shorthand routines added for "printmessage" and "question" messages;
  SendPrintMessage and SendQuestion repectively
- Display of status of fight with Officer Hardass cleaned up
- All servers are now interactive; to run in background simply attach
  standard input and output to /dev/null
- AI Player can now connect to server and perform simple actions
- Bank and Loan Shark display cleaned up
- Drug busts etc. now displayed all at once rather than singly
- High scores now maintained by server
- `print_price` replaced with `FormatPrice`
- LOGF macro now used for all server log messages
- Read in location of score files, server, port from ~/.dopewars
- Fixed bugs in player-player fighting code

# 1.3.7 - 1999-03-28
- Proper support for tipoffs and spies
- Discovered spies cannot now be shot if you don't have a gun...
- Option added for computer players (non-functional however)

# 1.3.6 - 1999-03-14
- BreakoffCombat routine added to terminate fights cleanly when one
  player runs away from a fight (under 1.3.5 defending player would
  just hang when this was done...)

# 1.3.5 - 1999-02-27
- Basic support for meeting other players; `E_MEETOTHER` event added
- Simple player-player fights allowed with the use of `E_WAITFIGHT`,
  `E_DEFEND` and `E_ATTACK` events 
- Two players with same name bug fixed
- "question" message extended; server now passes a list of allowed
  responses in the first "word" of message data

# 1.3.4 - 1999-02-25
- Client and virtual server now maintain completely separate lists of
  players
- GunShop now works properly; user can actually see what's going on!

# 1.3.3 - 1999-02-23
- Complete implementation of fighting with Officer Hardass
- `E_DOCTOR` event added to handle question "do you want a doctor to
  sew you up?" after killing Hardass
- Clients now handle list and endlist messages properly to display
  lists of current players on starting a game
- Minor bugfix to ensure game actually ends after the 31st
- Client now wipes price list on each jet to stop old prices
  flashing up between messages from the server

# 1.3.2 - 1999-02-22
- "subwayflash" message added
- OfferObject/RandomOffer split into separate event from OfficerHardass
- "smoke paraquat" also given separate event (`E_WEED`) and implemented
- Bank/LoanShark bugfixes
- Bugfix for drug price generation code
- Partial implementation of fighting with Officer Hardass

# 1.3.1 - 1999-02-21
- Drugs can now be bought and sold
- RandomOffer and OfferObject routines added to handle server-based
  random events ("a friend gives you..." etc.) and object offers ("do
  you want to buy a..." etc.) although "smoke paraquat?" doesn't
  work properly
- GunShop / LoanShark / Bank / Pub all handled by the server now
- Some networking bugfixes 

# 1.3.0 - 1999-02-20
- Development series (moving decision-making from client to server to
  improve multi-player games and cut down on cheating, in preparation
  for an OpenSource release)
- Simple implementation of a "virtual server" to handle the server-side
  stuff within a single-player game
- Splitting up of Dopewars into dopewars.c (init. code and utils);
  message.c (message-handling code); serverside.c (server-side code);
  clientside.c (client-side code)
- Drug prices now generated by server, not client - so synchronisation
  of turns (and drug prices) should be easy to implement in the future
- Minimal functionality - networking backbone only...

# 1.2.0 - 1999-02-13
- Stable release; some bugs in fighting code cleaned up

# 1.1.26 - 1999-02-13
- "PolicePresence" member is now read - when a fight is started, there
  is a finite chance (varies from location to location) that the
  perpetrator will get attacked by the police
- MinDrug and MaxDrug members added to Location struct - some locations
  may have a smaller range of drugs on offer than others

# 1.1.25 - 1999-02-11
- Added an "Inventory" struct to keep track of players' belongings
  and anything dropped during a fight; winner of a fight now gets
  whatever the other player dropped (guns and/or drugs)

# 1.1.24 - 1999-02-09
- Put in code to "finish" fights properly when one player escapes
- Attacking player is now told whether they hit the other player or
  not when in a fight

# 1.1.23 - 1999-02-03
- "Jet" command replaced with "Run" when in a fight
- "PolicePresence" member added to Location struct
- GunShop bug fixed (guns were taking up no space) 

# 1.1.22 - 1999-01-30
- Implemented very simple "shoot at another dealers" code; players, on
  arriving at a location where another dealer already is, can choose
  to attack (if they have any guns). The attacked player can then
  choose to return fire or run for it...

# 1.1.21 - 1999-01-29
- Added support for the "spy on another dealer" bitch errand

# 1.1.20 - 1999-01-29
- Added support for the "tip off another dealer to the cops" bitch errand
