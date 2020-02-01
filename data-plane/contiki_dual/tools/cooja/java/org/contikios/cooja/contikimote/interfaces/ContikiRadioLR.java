package org.contikios.cooja.contikimote.interfaces;

import org.apache.log4j.Logger;
import org.contikios.cooja.ClassDescription;
import org.contikios.cooja.Mote;
import org.contikios.cooja.MoteInterfaceHandler;
import org.contikios.cooja.contikimote.LongRangeInterface;

/**
 * Created by wjuni on 2016. 8. 16..
 */

/* hwijoon */
@ClassDescription("Radio (Long Range)")
public class ContikiRadioLR extends ContikiRadio implements LongRangeInterface{

    /**
     * Creates an interface to the radio at mote.
     *
     * @param mote Mote
     * @see Mote
     * @see MoteInterfaceHandler
     */
    private static Logger logger = Logger.getLogger(ContikiRadioLR.class);

    public ContikiRadioLR(Mote mote) {
        super(mote);
        logger.warn("Radio For Long Range Initialized");
    }

    /* Contiki mote interface support */
    public static String[] getCoreInterfaceDependencies() {
        return new String[] { "radio_interface" };
    }
}
