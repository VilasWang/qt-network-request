#ifndef TEST_ENVIRONMENT_H
#define TEST_ENVIRONMENT_H

#include <QObject>
#include <QtTest/QtTest>

// Pure (no-network) unit tests for {{var}} environment substitution.
class EnvironmentTests : public QObject
{
    Q_OBJECT

private slots:
    void testSubstituteBasic();
    void testSubstituteMissingLeftIntact();
    void testSubstituteUrlAndQueryParams();
    void testSubstituteHeaders();
    void testSubstituteBody();
    void testSubstituteSkipsBinary();
    void testSubstituteAuthFields();
    void testRequestContextBuilderEnvironment();
};

#endif // TEST_ENVIRONMENT_H
