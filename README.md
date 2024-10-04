https://community.kde.org/KIO_GDrive

Hello there, lonely wanderer and welcome to the magical land of Google Drive!


INSTALLATION
============

    $ git clone git://anongit.kde.org/kio-gdrive.git
    $ cd kio-gdrive
    $ mkdir build && cd build
    $ cmake -DCMAKE_INSTALL_PREFIX=`qtpaths --install-prefix` ..
    $ sudo make install
    $ kdeinit5 # or just re-login

Now you are ready to use the worker. Either click the "Network" button in Dolphin or run:

    $ kioclient5 exec gdrive:/


KNOWN ISSUES
============

GDocs file don't have file size
  Not our fault, Google Drive API simply does not return filesize of these files.
  I think it's because they are in the Google Docs format, so the size is irrelevant,
  since only GDocs can open them, and if we convert the files into .ODT or .DOCX or
  whatever else, the size is different (so we would have to measure it manually)

Folders have "Unknown" size
  We cannot provide size information on folders, so I guess this is actually implemented
  in KIO/Dolphin by simply listing each folder and showing the number of files. This
  is probably not done on remote filesystems to save bandwidth and improve performance,
  hence the size is "unknown".


TODO
===========

Open tasks are tracked here: https://phabricator.kde.org/tag/kio_gdrive/
