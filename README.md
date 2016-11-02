## Purpose

This sample demonstrates that closing a `libusb_device_handle` does not wake up
a synchronous call blocking on it.

From the main thread:

```c
int r = libusb_interrupt_transfer(handle, endpoint, data, sizeof(data),
                                  &transferred, 0 /* infinite timeout */);
```

Meanwhile, from another thread:

```c
libusb_close(handle);
```

The call to `libusb_interrupt_transfer` does not return when the handle is
closed that way.

## How to

Make sure `libusb-1.0-0-dev` is installed.

Then just `make` and run:

    make && ./sample

It selects the first mouse it finds, and prints something on events. In
parallel, after 5 seconds, it closes the handle.

As a result, we see that the call `libusb_interrupt_transfer` does not return.
