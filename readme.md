# Contribute to Swift-WALA


## Download Projects

```
mkdir swift-source
cd swift-source
git clone https://github.com/apple/swift
git clone https://github.com/wala/WALA
git clone https://github.com/themaplelab/swift-wala
```

## Build Dependencies


### WALA

```
cd ./WALA
./gradlew assemble
cd ..
```

### Swift

```
cd ./swift
./utils/update-checkout --clone --tag swift-4.1.1-RELEASE
./utils/build-script -d
cd ..
```


### Edit Swift-WALA Configurations

```
cd swift-wala/com.ibm.wala.cast.swift
cp gradle.properties.example gradle.properties
```

Edit `gradle.properties` and provide proper paths.


### Build Swift-WALA

```
./gradlew assemble
```

### Run The Program


- First you need to setup environment variables.

```
export WALA_PATH_TO_SWIFT_BUILD={path/to/your/swift/build/dir}
export WALA_DIR={path/to/your/wala/dir}
export SWIFT_WALA_DIR={path/to/your/swift-wala/dir}
```


- To run the Java code you need to pass java library path `-Djava.library.path="{swift-wala/dir}/com.ibm.wala.cast.swift/build/libs/translator/shared"`.


- Otherwise, you can run the standalone c++ code in `{swift-wala/dir/}/com.ibm.wala.cast.swift/swift-wala-translator/build/external-build/swiftWala/linux_x86-64/bin`.

```
./swift-wala-translator-standalone example.swift
```

### Troubleshoot

- `warning: libXrender.so.1, needed by libjvm.so`: install jvm  dependencies. For example, on Ubuntu `apt install libfontconfig1 libxrender1 libxi6 libgconf-2-4 libxtst6`.

