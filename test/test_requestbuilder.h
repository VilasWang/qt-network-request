#ifndef TEST_REQUESTBUILDER_H
#define TEST_REQUESTBUILDER_H

#include <QObject>
#include <QtTest/QtTest>

// Pure (no-network) unit tests for the RequestContextBuilder fluent API.
// AGENTS.md mandates constructing requests via the builder, so these tests
// lock down that the fluent setters populate RequestContext correctly.
class TestRequestBuilder : public QObject
{
    Q_OBJECT

private slots:
    void testBasicFields();
    void testHeaders();
    void testBehaviorFields();
    void testRetryAndRedirects();
    void testTaskAndSessionIds();
    void testConfigsAndUserContext();
    void testBuildConsumesBuilder();
    void testQueryParamBuilder();
    void testQueryParamsMapBuilder();
    void testAuthBuilder();
    void testBodyFormDataBuilder();
};

#endif // TEST_REQUESTBUILDER_H
