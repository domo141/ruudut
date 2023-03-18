
Look into the files for info.


Tiedostot itsessään kertovat itsestään aika selkeästi.


X-resources for e.g. < X-resurssiasetukset mm. > chomp-and-confirm-paste <lle>

  $ printf %s\\n \
      'URxvt*perl-lib: path/to/with-urxvt' \
      'URxvt*perl-ext-common: chomp-and-confirm-paste,clear' \
    | xrdb -merge -
