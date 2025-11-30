
TEMPLATE = subdirs
CONFIG -= ordered
SUBDIRS += qnetworkrequest samples tests
qnetworkrequest.file = source/QNetworkRequest.pro
tests.file = test/test.pro
tests.depends = qnetworkrequest
samples.depends = qnetworkrequest

OTHER_FILES += README.md
#EssentialDepends = 
#OptionalDepends =
