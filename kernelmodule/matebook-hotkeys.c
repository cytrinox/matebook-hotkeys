#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/wmi.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Vogelbacher");
MODULE_DESCRIPTION("A simple Linux driver for Matebook X hotkeys");
MODULE_VERSION("0.1");


#define PEGATRON_FILE KBUILD_MODNAME

//#define PEGATRON_DEVICE_ID "PTK0001" // TODO ?


#define PEGATRON_DEVICE_ID "PNP0C14"


#define MATEBOOK_WMI_GUID           "79142400-C6A3-40FA-BADB-8A2652834100" // GUID for MyWMIInitClass
#define MATEBOOK_WMI_EVENT_GUID     "59142400-C6A3-40FA-BADB-8A2652834100"


#define PEGATRON_WMI_GUID "79142400-C6A3-40FA-BADB-8A2652834100"

static int pegatron_acpi_add(struct acpi_device*);
static int pegatron_acpi_remove(struct acpi_device*);
static void pegatron_acpi_notify(struct acpi_device*, u32);


static const struct acpi_device_id pegatron_device_ids[] = {
    { PEGATRON_DEVICE_ID, 0 },
    { "", 0}
};
MODULE_DEVICE_TABLE(acpi, pegatron_device_ids);



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

        printk(KERN_INFO "ACPI button code: %d", code);

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


static struct platform_driver platform_driver = {
        .driver = {
            .name = PEGATRON_FILE,
            .owner = THIS_MODULE,
        },
};


static struct acpi_driver pegatron_acpi_driver = {
    .name =         "Pegatron ACPI",
    .class =        "Pegatron",
    .ids =          pegatron_device_ids,
    .flags =        ACPI_DRIVER_ALL_NOTIFY_EVENTS,
    .ops =          {
                        .add = pegatron_acpi_add,
                        .remove = pegatron_acpi_remove,
                        .notify = pegatron_acpi_notify,
                    },
    .owner =        THIS_MODULE,
};


#define PEGATRON_ACPI_INIT_CODE 0x55AA66BB

static int pegatron_acpi_add(struct acpi_device *dev) {
    pr_info("[Pegatron] ACPI_ADD called\n");

    acpi_status status = AE_OK;
    int error;

    pr_info("[Pegatron] Initializing acpi data\n");

    if (!acpi_has_method(dev->handle, "INIT")){
        dev_err(&dev->dev, "[Pegatron] INIT method not found on DSDT\n");
        return -ENODEV;
    }

    status = acpi_execute_simple_method(dev->handle, "INIT", 0 /* PEGATRON_ACPI_INIT_CODE */);
    if (ACPI_FAILURE(status)){
        dev_err(&dev->dev, "[Pegatron] error calling ACPI INIT method\n");
        return -ENODEV;
    }

    status = acpi_execute_simple_method(dev->handle, "NTFY", 0x02);
    if (ACPI_FAILURE(status)){
        dev_err(&dev->dev, "[Pegatron] error calling ACPI INIT method\n");
        return -ENODEV;
    }

    pr_info("Fully initialized\n");

    return 0;
}


static int pegatron_acpi_remove(struct acpi_device *dev) {
    pr_info("[Pegatron] ACPI_REMOVE called\n");
    return 0;
}

static void pegatron_acpi_notify(struct acpi_device *dev, u32 event) {
    pr_info("[Pegatron] ACPI_NOTIFY called: %u\n", event);
}



static void pegatron_stop_wmi(void) {
    wmi_remove_notify_handler(MATEBOOK_WMI_GUID);
    wmi_remove_notify_handler(MATEBOOK_WMI_EVENT_GUID);    
}


static int pegatron_start_wmi(void) {
    acpi_status status;



        struct bios_args args = {
            .arg0 = 0x1,
        };
        struct acpi_buffer input = { (acpi_size) sizeof(args), &args };
        struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };


        status = wmi_evaluate_method(PEGATRON_WMI_GUID, 0x01, 0x06, &input, &output);

        if (ACPI_FAILURE(status)) {
            const char *msg = acpi_format_exception(status);
            pr_err("Failed to evaluate method: '%s', exiting...\n", msg);
            return -1;
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
     return 0;
    

}




static int __init mbxhotkeys_init(void) {
    int result = 0;

    pr_info("[Pegatron] ACPI/WMI module loaded(GUID: %s)\n", MATEBOOK_WMI_GUID);

    result = platform_driver_register(&platform_driver);
    if (result < 0)
        return result;


    result = acpi_bus_register_driver(&pegatron_acpi_driver);
    if (result < 0) {
        pr_err("[Pegatron] Could not insert Pegatron device driver. Exiting...\n");
        platform_driver_unregister(&platform_driver);
        return -ENODEV;
    }

    pr_info("[Pegatron] Checking for WMI GUID information\n");

    if (!wmi_has_guid(PEGATRON_WMI_GUID)) {
        pr_err("[Pegatron] WMI information doesn't match. Exiting...\n");
        return -ENODEV;
    }

    result = pegatron_start_wmi();

    if(result != 0) {
        acpi_bus_unregister_driver(&pegatron_acpi_driver);
        platform_driver_unregister(&platform_driver);
        return -ENODEV;
    }

/*
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
*/
    pr_info("[Pegatron] ACPI/WMI module fully initialized\n");
    return result;
}


static void __exit mbxhotkeys_exit(void) {
    pegatron_stop_wmi();
    acpi_bus_unregister_driver(&pegatron_acpi_driver);
    platform_driver_unregister(&platform_driver);
    pr_info("Unloading matebook-hotkeys...\n");
    /*
    wmi_remove_notify_handler(MATEBOOK_WMI_GUID);
    wmi_remove_notify_handler(MATEBOOK_WMI_EVENT_GUID);
    */
}



module_init(mbxhotkeys_init);
module_exit(mbxhotkeys_exit);
