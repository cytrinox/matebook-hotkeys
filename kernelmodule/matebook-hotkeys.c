#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/wmi.h>
#include <linux/acpi.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Vogelbacher");
MODULE_DESCRIPTION("A simple Linux driver for Matebook X hotkeys");
MODULE_VERSION("0.1");


#define MATEBOOK_WMI_GUID           "79142400-C6A3-40FA-BADB-8A2652834100" // GUID for MyWMIInitClass
#define MATEBOOK_WMI_EVENT_GUID     "59142400-C6A3-40FA-BADB-8A2652834100"



/*
 * Event handler for WMI events
 */
static void mbx_wmi_notify(u32 value, void *context)
{
    struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
    union acpi_object *obj;
    acpi_status status;
    int code;

    status = wmi_get_event_data(value, &response);
    if (status != AE_OK) {
        pr_err("bad wmi event status 0x%x\n", status);
        return;
    }

    obj = (union acpi_object *)response.pointer;

    if (obj && obj->type == ACPI_TYPE_INTEGER) {
        code = obj->integer.value;

        printk(KERN_INFO "ACPI button code: %x", code);

        /*
                if (!sparse_keymap_report_event(xxx_wmi_input_dev, code, 1, true))
                pr_info("Unknown key %x pressed\n", code);
        */
    } else {
        pr_info("unknown ACPI event\n");
    }

    kfree(obj);
}


struct bios_args {
    u32 arg0;
};


static int __init mbxhotkeys_init(void) {
    acpi_status status;

    struct bios_args args = {
        .arg0 = 0x0, // Unknown what to set...
    };

    struct acpi_buffer input = { (acpi_size) sizeof(args), &args };
    //struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };


    pr_info("matebook-hotkey: init module\n");


    pr_info("Checking for WMI GUID information\n");

    if (!wmi_has_guid(MATEBOOK_WMI_GUID)) {
        pr_err("WMI information doesn't match. Exiting...\n");
        return -ENODEV;
    }


    status = wmi_install_notify_handler(MATEBOOK_WMI_GUID,
                                        mbx_wmi_notify, NULL);
    if (ACPI_FAILURE(status)) {
        pr_err("Failed to install WMI handler for %s\n", MATEBOOK_WMI_GUID);
        return -ENODEV;
    }


    status = wmi_install_notify_handler(MATEBOOK_WMI_EVENT_GUID,
                                        mbx_wmi_notify, NULL);
    if (ACPI_FAILURE(status)) {
        pr_err("Failed to install WMI handler for %s\n", MATEBOOK_WMI_EVENT_GUID);
        wmi_remove_notify_handler(MATEBOOK_WMI_GUID);
        return -ENODEV;
    }


    // Not sure if this call is required, BMF data provides a Init method,
    // maybe it's a good idea to call it.
    status = wmi_evaluate_method(MATEBOOK_WMI_GUID, 0x01,
                                 0x01, &input, NULL);

    if (ACPI_FAILURE(status)) {
        const char *msg = acpi_format_exception(status);
        pr_err("Failed to evaluate method: '%s', exiting...\n", msg);
        wmi_remove_notify_handler(MATEBOOK_WMI_GUID);
        wmi_remove_notify_handler(MATEBOOK_WMI_EVENT_GUID);
        return -ENODEV;
    }

    return 0;
}


static void __exit mbxhotkeys_exit(void) {
    pr_info("Unloading matebook-hotkeys...\n");
    wmi_remove_notify_handler(MATEBOOK_WMI_GUID);
    wmi_remove_notify_handler(MATEBOOK_WMI_EVENT_GUID);
}



module_init(mbxhotkeys_init);
module_exit(mbxhotkeys_exit);
