Plan to only keep actual useful translation information.

For further rationale about increasing the signal to noise ratio of translations see:
https://www.berrange.com/posts/2018/11/29/improved-translation-po-file-handling-by-ditching-gettext-autotools-integration/

ATM due to implicit autotools integration and seemingly no control over the Makefile.in.in,
it's currently unknown how to make these script commands automatic in the build.

Thus manually needs to be done occassionally. Still to determine best guidence how often/when this should occur.

    make update-po
    for x in `ls *.po`; do msgattrib --clear-fuzzy --no-location --empty --no-obsolete $x -o $x; done

Remove junk not necessary in files for revision control control such as ATM:

    for x in `ls *.po`; do sed -i /PO-Revision-Date:/d $x ; done
    for x in `ls *.po`; do sed -i /POT-Creation-Date:/d $x ; done
    for x in `ls *.po`; do sed -i /X-Launchpad-Export-Date:/d $x ; done
    for x in `ls *.po`; do sed -i /X-Generator:/d $x ; done

Manual import of translations from Launchpad - identify updated langauges, download .po file(s) or entire catalog.
Strip any unnecessary information if present (as the above including code locations), then manually compare and insert new/changed translations between the two files
- e.g. using a diff tool such as meld.

Occassionally update Launchpad with new viking.pot (although notionally Launchpad is set to auto sync,
this is possibly no longer necessary)

    cd po ; make viking.pot
    https://translations.launchpad.net/viking/trunk/+translations-upload
