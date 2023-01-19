BUILD_DIR=$1

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR
git clone https://github.com/Distrinet-TACOS/buildroot.git
git clone https://github.com/Distrinet-TACOS/buildroot-external-boundary.git
git clone https://github.com/AuthenticExecution/shared-secure-peripherals.git
make BR2_EXTERNAL=$PWD/buildroot-external-boundary/:$PWD/shared-secure-peripherals/ -C buildroot/ O=$PWD/output imx6q_sabrelite_defconfig