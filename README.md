# icbd - The ICB Server

_**NOTE: HEAD builds with CMake.**_

ICB (Internet Citizen's Band) is a renaming of Sean Casey's original
ForumNet (or fn) program. Much of the code here-in is Sean's, though
considerable bug-fixing and extensions have been made over the
years such that it may be somewhat unrecognizable from the original.
John Atwood deVries and Rich Dellaripa performed some significant
changes from Sean's sources. Jon Luini then took over as the main
maintainer of it around 1995 and made numerous bugfixes over the next
several years. In May of 2000, Michel Hoche-Mong performed some
organizational clean-up, created the autoconf files, added SSL handling,
and made a variety of bugfixes. In May of 2019, the code was imported
into Michel's github home at https://github.com/hoche/icbd.

All of the changes from the time Jon Luini took it over (approx 1995)
up to the time it was imported into github (v1.2c) are noted in the file
README.CHANGEHIST.

Since then:
  * UTF-8 support has been added for messages (but not usernames or groupnames).
  * The build system has been updated (CMake).
  * "brick" has been added (feature request)
  * SSL support has been added (but still has some bugs).

Building and installation instructions can be found in README.INSTALL.

TODO.md is a ToDo list for the developers.

General ICB resources are maintained at http://www.icb.net/
