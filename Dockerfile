# build wut
FROM devkitpro/devkitarm:20230526 AS ctrubuild

ENV PATH=$DEVKITARM/bin:$PATH

WORKDIR /
RUN git clone https://github.com/devkitPro/libctru
WORKDIR /libctru
RUN git checkout 0cbae436c860e816f21bb01549f745d3dc16537b
# patching in servent struct, which is required for openssl
# also patching out AF_INET6, as it messes with openssl compilation
RUN echo 'diff --git a/libctru/include/netdb.h b/libctru/include/netdb.h\n\
index 0f4b047..384318c 100644\n\
--- a/libctru/include/netdb.h\n\
+++ b/libctru/include/netdb.h\n\
@@ -15,6 +15,15 @@ struct hostent {\n\
   uint16_t h_length;       /* length of address */\n\
   char     **h_addr_list;  /* list of addresses from name server */\n\
 };\n\
+\n\
+struct servent\n\
+{\n\
+   char  *s_name;\n\
+   char **s_aliases;\n\
+   int    s_port;\n\
+   char  *s_proto;\n\
+};\n\
+\n #define h_addr h_addr_list[0] /* for backward compatibility */\n\
 \n #define AI_PASSIVE     0x01\n\
diff --git a/libctru/include/sys/socket.h b/libctru/include/sys/socket.h\n\
index edee7db..61db4a4 100644\n\
--- a/libctru/include/sys/socket.h\n\
+++ b/libctru/include/sys/socket.h\n\
@@ -11,7 +11,7 @@\n\
 \n #define AF_UNSPEC       PF_UNSPEC\n #define AF_INET         PF_INET\n\
-#define AF_INET6        PF_INET6\n\
+//#define AF_INET6        PF_INET6\n\
 \n #define SOCK_STREAM     1\n #define SOCK_DGRAM      2\n\
' >> ctrupatches.patch && git apply ctrupatches.patch

WORKDIR /libctru/libctru
RUN make -j$(nproc)
RUN make install
WORKDIR /

# set up builder image
FROM devkitpro/devkitarm:20230526 AS builder

RUN apt-get update && apt-get -y install --no-install-recommends tar autoconf automake libtool && rm -rf /var/lib/apt/lists/*
COPY --from=ctrubuild /opt/devkitpro/libctru /opt/devkitpro/libctru

# build SDL2
# devkitpro only provides SDL 1.2 for 3DS, we need to use SDL2
FROM builder AS sdlbuild

RUN git clone -b SDL2 --single-branch https://github.com/libsdl-org/SDL
WORKDIR /SDL
RUN mkdir build
WORKDIR /SDL/build

ENV ARCH "-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft"
ENV CFLAGS "-O3 -mword-relocations -ffunction-sections -fdata-sections ${ARCH}"
ENV CXXFLAGS "${CFLAGS} -fno-rtti -fno-exceptions -std=c++11"
ENV ASFLAGS	"${ARCH}"
ENV LDFLAGS	"${ARCH} -L${DEVKITPRO}/libctru/lib"

RUN cmake .. -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" -DCMAKE_INSTALL_PREFIX=$DEVKITPRO/portlibs/3ds -DCMAKE_BUILD_TYPE=Release
RUN make -j$(nproc) && make install
WORKDIR /

# build openssl
FROM builder AS opensslbuild
ARG openssl_ver=1.1.1u

RUN curl -LO https://www.openssl.org/source/openssl-$openssl_ver.tar.gz && mkdir /openssl && tar xf openssl-$openssl_ver.tar.gz -C /openssl --strip-components=1
WORKDIR /openssl

RUN echo 'diff --git a/Configurations/10-main.conf b/Configurations/10-main.conf\n\
index 61c6689..efe686a 100644\n\
--- a/Configurations/10-main.conf\n\
+++ b/Configurations/10-main.conf\n\
@@ -627,6 +627,28 @@ my %targets = (\n\
         shared_extension => ".so",\n\
     },\n\
 \n\
+### 3DS target\n\
+    "3ds" => {\n\
+        inherit_from     => [ "BASE_unix" ],\n\
+        CC               => "$ENV{DEVKITARM}/bin/arm-none-eabi-gcc",\n\
+        CXX              => "$ENV{DEVKITARM}/bin/arm-none-eabi-g++",\n\
+        AR               => "$ENV{DEVKITARM}/bin/arm-none-eabi-ar",\n\
+        CFLAGS           => picker(default => "-Wall",\n\
+                                   debug   => "-O0 -g",\n\
+                                   release => "-O3"),\n\
+        CXXFLAGS         => picker(default => "-Wall",\n\
+                                   debug   => "-O0 -g",\n\
+                                   release => "-O3"),\n\
+        LDFLAGS          => "-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -specs=3dsx.specs -L$ENV{DEVKITPRO}/libctru/lib",\n\
+        asflags          => "-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft",\n\
+        cflags           => add("-mword-relocations -ffunction-sections -fdata-sections -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft"),\n\
+        cxxflags         => add("-fno-rtti -fno-exceptions -std=c++11 -mword-relocations -ffunction-sections -fdata-sections -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft"),\n\
+        lib_cppflags     => "-DSO_KEEPALIVE=0x0008 -DOPENSSL_USE_NODELETE -DB_ENDIAN -DNO_SYS_UN_H -DNO_SYSLOG -D__3DS__ -I$ENV{DEVKITPRO}/libctru/include",\n\
+        ex_libs          => add("-lctru -lm"),\n\
+        bn_ops           => "BN_LLONG RC4_CHAR",\n\
+        asm_arch         => '"'"'armv6k'"'"',\n\
+    },\n\
+\n ####\n #### Variety of LINUX:-)\n ####\n\
diff --git a/crypto/rand/rand_unix.c b/crypto/rand/rand_unix.c\n\
index 0f45251..d303e8e 100644\n\
--- a/crypto/rand/rand_unix.c\n\
+++ b/crypto/rand/rand_unix.c\n\
@@ -202,6 +202,41 @@ void rand_pool_keep_random_devices_open(int keep)\n\
 {\n\
 }\n\
 \n\
+# elif defined(__3DS__)\n\
+\n\
+#include <3ds/svc.h>\n\
+\n\
+size_t rand_pool_acquire_entropy(RAND_POOL *pool)\n\
+{\n\
+    int i;\n\
+    size_t bytes_needed;\n\
+    unsigned char v;\n\
+\n\
+    bytes_needed = rand_pool_bytes_needed(pool, 4 /*entropy_factor*/);\n\
+\n\
+    for (i = 0; i < bytes_needed; i++) {\n\
+        srand(svcGetSystemTick());\n\
+        v = rand() & 0xff;\n\
+\n\
+        rand_pool_add(pool, &v, sizeof(v), 2);\n\
+    }\n\
+\n\
+    return rand_pool_entropy_available(pool);\n\
+}\n\
+\n\
+int rand_pool_init(void)\n\
+{\n\
+    return 1;\n\
+}\n\
+\n\
+void rand_pool_cleanup(void)\n\
+{\n\
+}\n\
+\n\
+void rand_pool_keep_random_devices_open(int keep)\n\
+{\n\
+}\n\
+\n # else\n\
 \n #  if defined(OPENSSL_RAND_SEED_EGD) && \\\n\
diff --git a/crypto/uid.c b/crypto/uid.c\n\
index a9eae36..4a81d98 100644\n\
--- a/crypto/uid.c\n\
+++ b/crypto/uid.c\n\
@@ -10,7 +10,7 @@\n #include <openssl/crypto.h>\n #include <openssl/opensslconf.h>\n\
 \n\
-#if defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VXWORKS) || defined(OPENSSL_SYS_UEFI)\n\
+#if defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_VXWORKS) || defined(OPENSSL_SYS_UEFI) || defined(__3DS__)\n\
 \n\
 int OPENSSL_issetugid(void)\n\
 {\n\
' >> 3ds.patch && git apply 3ds.patch

RUN ./Configure 3ds \
  no-threads no-shared no-asm no-ui-console no-unit-test no-tests no-buildtest-c++ no-external-tests no-autoload-config \
  --with-rand-seed=os -static

RUN make build_generated
RUN make libssl.a libcrypto.a -j$(nproc)
WORKDIR /

# build expat
FROM builder as expatbuild
ARG expat_tag=2_5_0
ARG expat_ver=2.5.0

RUN curl -LO https://github.com/libexpat/libexpat/releases/download/R_$expat_tag/expat-$expat_ver.tar.gz && mkdir /expat && tar xf expat-$expat_ver.tar.gz -C /expat --strip-components=1
WORKDIR /expat

ENV ARCH "-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft"
ENV CFLAGS "-O3 -mword-relocations -ffunction-sections -fdata-sections ${ARCH}"
ENV CXXFLAGS "${CFLAGS} -fno-rtti -fno-exceptions -std=c++11"
ENV CPPFLAGS "-D__3DS__ -I${DEVKITPRO}/libctru/include"
ENV ASFLAGS	"${ARCH}"
ENV LDFLAGS	"${ARCH} -specs=3dsx.specs -L${DEVKITPRO}/libctru/lib"
ENV LIBS "-lctru -lm"

RUN autoreconf -fi
RUN ./configure \
--prefix=$DEVKITPRO/portlibs/3ds/ \
--host=arm-none-eabi \
--enable-static \
--without-examples \
--without-tests \
--without-docbook \
CC=$DEVKITARM/bin/arm-none-eabi-gcc \
AR=$DEVKITARM/bin/arm-none-eabi-ar \
RANLIB=$DEVKITARM/bin/arm-none-eabi-ranlib \
PKG_CONFIG=$DEVKITPRO/portlibs/3ds/bin/arm-none-eabi-pkg-config

RUN make -j$(nproc) && make install
WORKDIR /

# download build utils
FROM builder as tooldownload

ARG bannertool_tag=1.2.0
ARG makerom_tag=0.18.3

RUN curl -LO "https://github.com/Steveice10/bannertool/releases/download/${bannertool_tag}/bannertool.zip" && mkdir /bannertool && unzip bannertool.zip -d /bannertool
RUN curl -LO "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v${makerom_tag}/makerom-v${makerom_tag}-ubuntu_x86_64.zip" && mkdir /makerom && unzip "makerom-v${makerom_tag}-ubuntu_x86_64.zip" -d /makerom && chmod +x /makerom/makerom

# build final container
FROM devkitpro/devkitarm:20230526 AS final

# copy in libctru
COPY --from=ctrubuild /opt/devkitpro/libctru /opt/devkitpro/libctru

# copy in SDL2
COPY --from=sdlbuild /opt/devkitpro/portlibs/3ds/lib/libSDL2.a /opt/devkitpro/portlibs/3ds/lib/
COPY --from=sdlbuild /opt/devkitpro/portlibs/3ds/include/SDL2 /opt/devkitpro/portlibs/3ds/include/SDL2/

# copy in openssl
COPY --from=opensslbuild /openssl/libcrypto.a /openssl/libssl.a /opt/devkitpro/portlibs/3ds/lib/
COPY --from=opensslbuild /openssl/include/openssl /opt/devkitpro/portlibs/3ds/include/openssl/
COPY --from=opensslbuild /openssl/include/crypto /opt/devkitpro/portlibs/3ds/include/crypto/

# copy in expat
COPY --from=expatbuild /opt/devkitpro/portlibs/3ds/lib/libexpat.a /opt/devkitpro/portlibs/3ds/lib/
COPY --from=expatbuild /opt/devkitpro/portlibs/3ds/include/expat.h /opt/devkitpro/portlibs/3ds/include/expat.h
COPY --from=expatbuild /opt/devkitpro/portlibs/3ds/include/expat_config.h /opt/devkitpro/portlibs/3ds/include/expat_config.h
COPY --from=expatbuild /opt/devkitpro/portlibs/3ds/include/expat_external.h /opt/devkitpro/portlibs/3ds/include/expat_external.h

# copy in build utils
COPY --from=tooldownload /bannertool/linux-x86_64/bannertool /usr/bin/bannertool
COPY --from=tooldownload /makerom/makerom /usr/bin/makerom

WORKDIR /project
