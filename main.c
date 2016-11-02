#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

// Sample to demonstrate that closing a device handle (from another thread)
// does not wake up a synchronous call (libusb_interrupt_transfer)

static const int USB_INTERFACE_SUBCLASS_BOOT = 1;
static const int USB_INTERFACE_PROTOCOL_MOUSE = 2;

int is_mouse(const struct libusb_interface_descriptor *desc) {
	return desc->bInterfaceClass == LIBUSB_CLASS_HID &&
	    desc->bInterfaceSubClass == USB_INTERFACE_SUBCLASS_BOOT &&
	    desc->bInterfaceProtocol == USB_INTERFACE_PROTOCOL_MOUSE;
}

libusb_device *get_mouse(libusb_device **list, int size, uint8_t *interface, uint8_t *endpoint, uint16_t *max_packet_size) {
	for (int i = 0; i < size; ++i) {
		libusb_device *device = list[i];
		struct libusb_config_descriptor *config;
		int r = libusb_get_active_config_descriptor(device, &config);
		if (r < 0) {
			fprintf(stderr, "Cannot retrieve config descriptor: %s\n", libusb_error_name(r));
			return NULL;
		}
		for (uint8_t j = 0; j < config->bNumInterfaces; ++j) {
			const struct libusb_interface *itf = &config->interface[j];
			for (uint8_t k = 0; k < itf->num_altsetting; ++k) {
				const struct libusb_interface_descriptor *itf_desc = &itf->altsetting[k];
				if (is_mouse(itf_desc)) {
					// a mouse should have only one input endpoint
					const struct libusb_endpoint_descriptor *ep_desc = &itf_desc->endpoint[0];
					*interface = itf_desc->bInterfaceNumber;
					*endpoint = ep_desc->bEndpointAddress;
					*max_packet_size = ep_desc->wMaxPacketSize;
					libusb_free_config_descriptor(config);
					return device;
				}
			}
		}

		libusb_free_config_descriptor(config);
	}
	return NULL;
}

void *run(void *arg) {
	libusb_device_handle *handle = arg;
	sleep(5);
	printf("Closing device handle\n");
	libusb_close(handle);
	return NULL;
}

int main(void) {
	libusb_device **list;
	int ret = 0;

	libusb_init(NULL);
	ssize_t cnt = libusb_get_device_list(NULL, &list);
	if (cnt < 0) {
		fprintf(stderr, "Cannot retrieve USB devices: %s\n", libusb_error_name(cnt));
		return 1;
	}

	uint8_t interface;
	uint8_t endpoint;
	uint16_t max_packet_size;
	libusb_device *device = get_mouse(list, cnt, &interface, &endpoint, &max_packet_size);
	if (!device) {
		fprintf(stderr, "Device not found\n");
		ret = 2;
		goto cleanup;
	}

	struct libusb_device_descriptor desc;
	libusb_get_device_descriptor(device, &desc);
	printf("Selected device %04x:%04x\n", desc.idVendor, desc.idProduct);

	libusb_device_handle *handle;
	int r = libusb_open(device, &handle);
	if (r != LIBUSB_SUCCESS) {
		fprintf(stderr, "Cannot open device: %s\n", libusb_error_name(r));
		ret = 3;
		goto cleanup;
	}

	r = libusb_set_auto_detach_kernel_driver(handle, 1);
	if (r != LIBUSB_SUCCESS) {
		fprintf(stderr, "Cannot set auto-detach kernel driver: %s\n", libusb_error_name(r));
		ret = 4;
		goto cleanup;
	}

	r = libusb_claim_interface(handle, interface);
	if (r != LIBUSB_SUCCESS) {
		fprintf(stderr, "Cannot claim interface: %s\n", libusb_error_name(r));
		ret = 5;
		goto cleanup;
	}

	pthread_t thread;
	if (pthread_create(&thread, NULL, run, handle)) {
		fprintf(stderr, "Cannot create thread");
		ret = 6;
		goto cleanup;
	}

	int i = 0;
	for (;;) {
		unsigned char data[max_packet_size];
		int transferred;
		printf("... %d\n", i);
		r = libusb_interrupt_transfer(handle, endpoint, data, sizeof(data), &transferred, 0);
		printf("+++ %d\n", i++);
		if (r) {
			fprintf(stderr, "Interrupt transfer failed: %s\n", libusb_error_name(r));
			ret = 7;
			goto cleanup2;
		}
	}

cleanup2:
	pthread_cancel(thread);
	pthread_join(thread, NULL);
cleanup:
	libusb_free_device_list(list, 1);
	return ret;
}
