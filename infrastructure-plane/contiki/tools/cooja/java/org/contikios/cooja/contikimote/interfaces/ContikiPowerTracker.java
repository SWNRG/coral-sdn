
package org.contikios.cooja.contikimote.interfaces;

import org.apache.log4j.Logger;
import org.contikios.cooja.*;
import org.contikios.cooja.contikimote.ContikiMoteInterface;
import org.contikios.cooja.interfaces.PolledAfterActiveTicks;
import org.contikios.cooja.mote.memory.VarMemory;
import org.jdom.Element;

import javax.swing.*;
import java.util.Collection;


@ClassDescription("Power Tracker Module")
public class ContikiPowerTracker extends MoteInterface implements ContikiMoteInterface, PolledAfterActiveTicks {
  private static Logger logger = Logger.getLogger(ContikiPowerTracker.class);

  private Mote mote = null;
  private VarMemory moteMem = null;

  public ContikiPowerTracker() {
  }


  public ContikiPowerTracker(Mote mote) {
    this.mote = mote;
    this.moteMem = new VarMemory(mote.getMemory());
  }

  public void doActionsAfterTick() {
    /* HJ : update powertracker info */
    updatePowerTrackerInfo();
  }

  private void setMemory(String varname, long value){
    /* for uint64_t support
    for (int i=0; i< 4; i++)
      moteMem.setByteValueOf(addr+i, (byte)((value >> i) & 0xffL));
    */
    /* convert us to ms */
    moteMem.setInt32ValueOf(varname, (int)(value/1000));
  }


  private void updatePowerTrackerInfo(){
    MotePowerInfoEntry mpie = this.mote.getSimulation().motepowerinfo.getMotePowerEntry(this.mote.getID());
    if(mpie != null){
      setMemory("COOJA_radioOn", mpie.radioOn);
      setMemory("COOJA_radioTx", mpie.radioTx);
      setMemory("COOJA_radioRx", mpie.radioRx);
      setMemory("COOJA_radioOnLong", mpie.radioOnLong);
      setMemory("COOJA_radioTxLong", mpie.radioTxLong);
      setMemory("COOJA_radioRxLong", mpie.radioRxLong);
      setMemory("COOJA_duration", mpie.duration);
    }
  }

  @Override
  public JPanel getInterfaceVisualizer() {
    return null;
  }

  @Override
  public void releaseInterfaceVisualizer(JPanel panel) {

  }

  @Override
  public Collection<Element> getConfigXML() {
    return null;
  }

  @Override
  public void setConfigXML(Collection<Element> configXML, boolean visAvailable) {

  }

	public static String[] getCoreInterfaceDependencies() {
		return new String[]{"powertracker_interface"};
	}
}
