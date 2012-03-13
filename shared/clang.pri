CLANG_ROOT=$$(CLANG_ROOT)
isEmpty(CLANG_ROOT):CLANG_ROOT=/usr/local
CLANG_LIBS = $${CLANG_ROOT}/lib
CLANG_INCLUDE = $${CLANG_ROOT}/include
LIBS += -lclang -lcrypto -L../3rdparty -lleveldb -lz -llzma -L$${CLANG_LIBS} -Wl,-rpath,$${CLANG_LIBS}
INCLUDEPATH += $${CLANG_INCLUDE}
CLANG_VERSION=$$(CLANG_VERSION)
isEmpty(CLANG_VERSION):CLANG_VERSION=3.0
CLANG_RUNTIME_INCLUDE = $${CLANG_ROOT}/lib/clang/$${CLANG_VERSION}/include
DEFINES+="CLANG_RUNTIME_INCLUDE=\\\"$${CLANG_RUNTIME_INCLUDE}\\\""
