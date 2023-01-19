REPO          ?= authexec/event-manager-trustzone
TAG           ?= latest
OPTEE_DIR     ?= /opt/optee
IMX_DIR       ?= imx
PACKAGE       ?= optee-examples

NW_DEVICE     ?= /dev/ttyUSB1
SW_DEVICE     ?= /dev/ttyUSB0
PORT          ?= 1236

build:
	docker build -t $(REPO):$(TAG) .

push: login
	docker push $(REPO):$(TAG)

pull:
	docker pull $(REPO):$(TAG)

run_qemu: check_port
	docker run --rm -v $(OPTEE_DIR):/opt/optee -e PORT=$(PORT) -p $(PORT):1236 --name event-manager-$(PORT) $(REPO):$(TAG)

init_imx:
	scripts/init-imx.sh $(NW_DEVICE) $(SW_DEVICE)

run_imx:
	docker run --rm -e IMX=1 -e PORT=$(PORT) --device=$(NW_DEVICE):/dev/NW --device=$(SW_DEVICE):/dev/SW -d --name event-manager-imx authexec/event-manager-trustzone

login:
	docker login

check_port:
	@test $(PORT) || (echo "PORT variable not defined. Run make <target> PORT=<port>" && return 1)

imx_setup:
	scripts/imx_setup.sh $(IMX_DIR)

imx_build:
	make -C $(IMX_DIR)/output BR2_JLEVEL="$(nproc)" all

imx_rebuild:
	make -C $(IMX_DIR)/output $(PACKAGE)-dirclean
	make -C $(IMX_DIR)/output BR2_JLEVEL="$(nproc)" $(PACKAGE)-rebuild all

clean:
	rm -rf imx