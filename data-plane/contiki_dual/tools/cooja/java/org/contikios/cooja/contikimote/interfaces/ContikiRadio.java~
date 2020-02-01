/*
 * Copyright (c) 2008, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

package org.contikios.cooja.contikimote.interfaces;

import java.util.ArrayList;
import java.util.Collection;

import org.apache.log4j.Logger;
import org.contikios.cooja.*;
import org.contikios.cooja.contikimote.LongRangeInterface;
import org.jdom.Element;

import org.contikios.cooja.mote.memory.SectionMoteMemory;
import org.contikios.cooja.contikimote.ContikiMote;
import org.contikios.cooja.contikimote.ContikiMoteInterface;
import org.contikios.cooja.interfaces.PolledAfterActiveTicks;
import org.contikios.cooja.interfaces.Position;
import org.contikios.cooja.interfaces.Radio;
import org.contikios.cooja.mote.memory.VarMemory;
import org.contikios.cooja.radiomediums.UDGM;

/**
 * Packet radio transceiver mote interface.
 *
 * To simulate transmission rates, the underlying Contiki system is
 * locked in TX or RX states using multi-threading library.
 *
 * Contiki variables:
 * <ul>
 * <li>char simReceiving (1=mote radio is receiving)
 * <li>char simInPolled
 * <p>
 * <li>int simInSize (size of received data packet)
 * <li>byte[] simInDataBuffer (data of received data packet)
 * <p>
 * <li>int simOutSize (size of transmitted data packet)
 * <li>byte[] simOutDataBuffer (data of transmitted data packet)
 * <p>
 * <li>char simRadioHWOn (radio hardware status (on/off))
 * <li>int simSignalStrength (heard radio signal strength)
 * <li>int simLastSignalStrength
 * <li>char simPower (number indicating power output)
 * <li>int simRadioChannel (number indicating current channel)
 * </ul>
 * <p>
 *
 * Core interface:
 * <ul>
 * <li>radio_interface
 * </ul>
 * <p>
 *
 * This observable notifies at radio state changes during RX and TX.
 *
 * @see #getLastEvent()
 * @see UDGM
 *
 * @author Fredrik Osterlind
 */
@ClassDescription("Radio (Short Range)")
public class ContikiRadio extends Radio implements ContikiMoteInterface, PolledAfterActiveTicks {
  private ContikiMote mote;

  private VarMemory myMoteMemory;

  private static Logger logger = Logger.getLogger(ContikiRadio.class);

  /**
   * Transmission bitrate (kbps).
   */
  private double RADIO_TRANSMISSION_RATE_kbps;

  private RadioPacket packetToMote = null;

  private RadioPacket packetFromMote = null;

  private boolean radioOn = true;

  private boolean isTransmitting = false;

  private boolean isInterfered = false;

  private long transmissionEndTime = -1;

  private RadioEvent lastEvent = RadioEvent.UNKNOWN;

  private long lastEventTime = 0;

  private int oldOutputPowerIndicator = -1;

  private int oldRadioChannel = -1;

  /**
   * Creates an interface to the radio at mote.
   *
   * @param mote Mote
   *
   * @see Mote
   * @see org.contikios.cooja.MoteInterfaceHandler
   */

  public ContikiRadio(Mote mote) {
    if(!(this instanceof LongRangeInterface))
      logger.warn("Radio For Short Range Initialized");
    // Read class configurations of this mote type
    RADIO_TRANSMISSION_RATE_kbps = mote.getType().getConfig().getDoubleValue(
        ContikiRadio.class, "RADIO_TRANSMISSION_RATE_kbps");

    this.mote = (ContikiMote) mote;
    this.myMoteMemory = new VarMemory(mote.getMemory());

    radioOn = myMoteMemory.getByteValueOf(getSymbolNameLR("simRadioHWOn")) == 1;
  }

  public void setLongRangeReceivingMode(boolean val){
      if(val)
          myMoteMemory.setIntValueOf("LongRangeReceiving", 1);
      else
          myMoteMemory.setIntValueOf("LongRangeReceiving", 0);
  }


  /* hwijoon */
  private String getSymbolNameLR(String symbol_pre){
    if(this instanceof LongRangeInterface)
        return symbol_pre + "LR";
    return symbol_pre;
  }

  /* Contiki mote interface support */
  public static String[] getCoreInterfaceDependencies() {
    return new String[] { "radio_interface" };
  }

  /* Packet radio support */
  public RadioPacket getLastPacketTransmitted() {
    return packetFromMote;
  }

  public RadioPacket getLastPacketReceived() {
    return packetToMote;
  }

  public void setReceivedPacket(RadioPacket packet) {
    packetToMote = packet;
  }

  /* General radio support */
  public boolean isRadioOn() {
    return radioOn;
  }

  public boolean isTransmitting() {
    return isTransmitting;
  }

  public boolean isReceiving() {
      return myMoteMemory.getByteValueOf(getSymbolNameLR("simReceiving")) == 1;
              // && (this instanceof LongRangeInterface) == (myMoteMemory.getIntValueOf("LongRangeReceiving") == 1);
  }

  public boolean isInterfered() {
    return isInterfered;
  }

  public int getChannel() {
    return myMoteMemory.getIntValueOf(getSymbolNameLR("simRadioChannel"));
  }

  /* hwijoon */
  public boolean isLongRangeMode() {
    boolean b = myMoteMemory.getIntValueOf("LongRangeTransmit") == 1;
    return b;
  }


  public void signalReceptionStart() {
      /* hwijoon */
    setLongRangeReceivingMode(this instanceof LongRangeInterface);
    // logger.warn("Reception starts, is it longrange? : "+(this instanceof LongRangeInterface));
    packetToMote = null;
//    logger.warn("mote:"+this.mote+" Inter, Recv, Trans " + isInterfered() +" " + isReceiving() + " " + isTransmitting());
    if (isInterfered() || isReceiving() || isTransmitting()) {
      interfereAnyReception();
      return;
    }
    // therefore cannot handle both receiving and transmitting
    // might have some sync issues (isReceiving() and check simReceiving is not atomic)
    myMoteMemory.setByteValueOf(getSymbolNameLR("simReceiving"), (byte) 1);
    mote.requestImmediateWakeup();

    lastEventTime = mote.getSimulation().getSimulationTime();
    lastEvent = RadioEvent.RECEPTION_STARTED;

    this.setChanged();
    this.notifyObservers();
  }

  public void signalReceptionEnd() {
    // logger.warn("Reception ends, is it longrange? : "+(this instanceof LongRangeInterface));
//      logger.warn("mote:"+this.mote+" isInterfered " + isInterfered);
      if (isInterfered || packetToMote == null) {
      isInterfered = false;
      packetToMote = null;
      myMoteMemory.setIntValueOf(getSymbolNameLR("simInSize"), 0);
    } else {
      myMoteMemory.setIntValueOf(getSymbolNameLR("simInSize"), packetToMote.getPacketData().length);
      myMoteMemory.setByteArray(getSymbolNameLR("simInDataBuffer"), packetToMote.getPacketData());
    }
    myMoteMemory.setByteValueOf(getSymbolNameLR("simReceiving"), (byte) 0);
    mote.requestImmediateWakeup();
    lastEventTime = mote.getSimulation().getSimulationTime();
    lastEvent = RadioEvent.RECEPTION_FINISHED;
//    logger.warn("signalRecvEnd mote:"+this.mote + "time:" + lastEventTime + "Insize:" + myMoteMemory.getIntValueOf(getSymbolNameLR("simInSize")));
    this.setChanged();
    this.notifyObservers();
  }

  public RadioEvent getLastEvent() {
    return lastEvent;
  }

  public void interfereAnyReception() {
    if (isInterfered()) {
      return;
    }

    isInterfered = true;

    lastEvent = RadioEvent.RECEPTION_INTERFERED;
    lastEventTime = mote.getSimulation().getSimulationTime();
//    logger.warn("mote:"+this.mote+" time:"+lastEventTime+" interfereAnyReception");
    this.setChanged();
    this.notifyObservers();
  }

  public double getCurrentOutputPower() {
    /* TODO Implement method */
    logger.warn("Not implemented, always returning 0 dBm");
    return 0;
  }

  public int getOutputPowerIndicatorMax() {
    return 100;
  }

  public int getCurrentOutputPowerIndicator() {
    return myMoteMemory.getByteValueOf(getSymbolNameLR("simPower"));
  }

  public double getCurrentSignalStrength() {
    return myMoteMemory.getIntValueOf(getSymbolNameLR("simSignalStrength"));
  }

  public void setCurrentSignalStrength(double signalStrength) {
    myMoteMemory.setIntValueOf(getSymbolNameLR("simSignalStrength"), (int) signalStrength);
  }

  /** Set LQI to a value between 0 and 255.
   *
   * @see org.contikios.cooja.interfaces.Radio#setLQI(int)
   */
  public void setLQI(int lqi){
    if(lqi<0) {
      lqi=0;
    }
    else if(lqi>0xff) {
      lqi=0xff;
    }
    myMoteMemory.setIntValueOf(getSymbolNameLR("simLQI"), lqi);
  }

  public int getLQI(){
    return myMoteMemory.getIntValueOf(getSymbolNameLR("simLQI"));
  }

  public Position getPosition() {
    return mote.getInterfaces().getPosition();
  }

  public void doActionsAfterTick() {
    boolean isThisIntfLongRange = false;
    if(this instanceof LongRangeInterface)
      isThisIntfLongRange = true;


    long now = mote.getSimulation().getSimulationTime();

    /* Check if radio hardware status changed */
    if (radioOn != (myMoteMemory.getByteValueOf(getSymbolNameLR("simRadioHWOn")) == 1)) {
      radioOn = !radioOn;
//      logger.warn("mote:"+this.mote+" now: " + now + " radio state changed: "+radioOn);
      if (!radioOn) {
        myMoteMemory.setByteValueOf(getSymbolNameLR("simReceiving"), (byte) 0);
        myMoteMemory.setIntValueOf(getSymbolNameLR("simInSize"), 0);
        myMoteMemory.setIntValueOf(getSymbolNameLR("simOutSize"), 0);
        isTransmitting = false;
        lastEvent = RadioEvent.HW_OFF;
      } else {
        lastEvent = RadioEvent.HW_ON;
      }

      lastEventTime = now;
      this.setChanged();
      this.notifyObservers();
    }
    if (!radioOn) {
      return;
    }

    /* Check if radio output power changed */
    if (myMoteMemory.getByteValueOf(getSymbolNameLR("simPower")) != oldOutputPowerIndicator) {
      oldOutputPowerIndicator = myMoteMemory.getByteValueOf(getSymbolNameLR("simPower"));
      lastEvent = RadioEvent.UNKNOWN;
      this.setChanged();
      this.notifyObservers();
    }

    /* Check if radio channel changed */
    if (getChannel() != oldRadioChannel) {
      oldRadioChannel = getChannel();
      lastEvent = RadioEvent.UNKNOWN;
      this.setChanged();
      this.notifyObservers();
    }


    /* Ongoing transmission */
    /* hwijoon */
    // logger.warn("########## Is it in Longrange mode? " + isLongRangeMode()+ "// This instance of Longrange" + (this instanceof LongRangeInterface) + "  ###############");
    if(isLongRangeMode() == (this instanceof LongRangeInterface)){
      // logger.warn("LongRangeMode = " + isLongRangeMode() + ", R=" + this);
      if (isTransmitting && now >= transmissionEndTime) {
        myMoteMemory.setIntValueOf(getSymbolNameLR("simOutSize"), 0);
        isTransmitting = false;
        mote.requestImmediateWakeup();

        lastEventTime = now;
        lastEvent = RadioEvent.TRANSMISSION_FINISHED;
        this.setChanged();
        this.notifyObservers();
        //logger.warn("Transmission Finished, Mote = " + this.mote + "LR = " + isLongRangeMode());
      }

      /* New transmission */
      int size = myMoteMemory.getIntValueOf(getSymbolNameLR("simOutSize"));
      if (!isTransmitting && size > 0) {
        //        logger.warn("Packet Transmission Detected by " + this);

        //logger.warn("Transmission Initiated, Mote = " + this.mote + "LR = " + isLongRangeMode());
          packetFromMote = new COOJARadioPacket(myMoteMemory.getByteArray(getSymbolNameLR("simOutDataBuffer"), size));

          /* JOONKI */
//           logger.warn("\n\nTransmitting packet is " + new String(myMoteMemory.getByteArray(getSymbolNameLR("simOutDataBuffer"),size)));
//           logger.warn("LongRangeMode = " + isLongRangeMode() + ", R=" + this);
//           logger.warn("This instanceof Longrange = " +(this instanceof LongRangeInterface));
          if (packetFromMote.getPacketData() == null || packetFromMote.getPacketData().length == 0) {
            logger.warn("Skipping zero sized Contiki packet (no buffer)");
            myMoteMemory.setIntValueOf(getSymbolNameLR("simOutSize"), 0);
            mote.requestImmediateWakeup();
            return;
          }

          isTransmitting = true;

      /* Calculate transmission duration (us) */
      /* XXX Currently floored due to millisecond scheduling! */
          
					long duration = (int) (Simulation.MILLISECOND*((8 * size /*bits*/) / RADIO_TRANSMISSION_RATE_kbps));

    			if(isLongRangeMode() == true){
						duration = duration * 5; /* Long range Tx duration */
					}

          transmissionEndTime = now + Math.max(1, duration);

          lastEventTime = now;
          lastEvent = RadioEvent.TRANSMISSION_STARTED;
          this.setChanged();
          this.notifyObservers();
          //logger.debug("----- NEW CONTIKI TRANSMISSION DETECTED -----");

          // Deliver packet right away
          lastEvent = RadioEvent.PACKET_TRANSMITTED;
          this.setChanged();
          this.notifyObservers();
          //logger.debug("----- CONTIKI PACKET DELIVERED -----");
      }

    }



    if (isTransmitting && transmissionEndTime > now) {
      mote.scheduleNextWakeup(transmissionEndTime);
    }
  }

  public Collection<Element> getConfigXML() {
           ArrayList<Element> config = new ArrayList<Element>();

           Element element;

           /* Radio bitrate */
           element = new Element("bitrate");
           element.setText("" + RADIO_TRANSMISSION_RATE_kbps);
           config.add(element);

           return config;
  }

  public void setConfigXML(Collection<Element> configXML,
                 boolean visAvailable) {
         for (Element element : configXML) {
                 if (element.getName().equals("bitrate")) {
                         RADIO_TRANSMISSION_RATE_kbps = Double.parseDouble(element.getText());
                         logger.info("Radio bitrate reconfigured to (kbps): " + RADIO_TRANSMISSION_RATE_kbps);
                 }
         }
  }

  public Mote getMote() {
    return mote;
  }

  public String toString() {
    return "Radio at " + mote;
  }
}
