package Controller;
import java.util.Arrays;
import java.util.List;
import java.util.Random;

import weka.core.Instance;
import weka.core.Instances;
import weka.core.DenseInstance;
import weka.core.converters.ConverterUtils.DataSource;
import weka.filters.Filter;
import weka.filters.unsupervised.attribute.Remove;
import weka.classifiers.Classifier;
import weka.classifiers.trees.J48;
import weka.classifiers.Evaluation;

public class simpleWekaClassifierExample  {
	static Instances data;
	Classifier cls;
	simpleWekaClassifierExample() {
		// Build a model on all data.
		
		// Import the training dataset.
		DataSource source;
		try {
			source = new DataSource("./jsidata/dataset-10-coral.arff");
			data = source.getDataSet();
		} catch (Exception e) {
			// TODO Auto-generated catch block
			System.out.println("WEKA intialization error OR data file not found");
			e.printStackTrace();
		}

		
		// Remove unwanted features.
		String unwantedAttributesArgument = "";
		List<String> unwantedAttributes = 
			Arrays.asList("prr", "seq", "received", "link_num", "experiment_num", "dataset_num");
		for(int attributeIndex = 0; attributeIndex < data.numAttributes(); attributeIndex++) {
			if(unwantedAttributes.contains(data.attribute(attributeIndex).name())) {
				unwantedAttributesArgument += Integer.toString(attributeIndex + 1) + ",";
			}
		}
		Remove removeUnwanted = new Remove();
		removeUnwanted.setAttributeIndices(unwantedAttributesArgument);
		try {
			removeUnwanted.setInputFormat(data);
			data = Filter.useFilter(data, removeUnwanted);
		} catch (Exception e) {
			System.out.println("ERROR: WEKA filter error");
			e.printStackTrace();
		}

		
		// Find and set class index.
		for(int attributeIndex = 0; attributeIndex < data.numAttributes(); attributeIndex++) {
			if(data.attribute(attributeIndex).name().equals("class")) {
				data.setClassIndex(attributeIndex);
				break;
			}
		}
		
		// Train the classifier.
		try {
			cls = new J48();
			cls.buildClassifier(data);
			System.out.println(cls); // Print the decision tree.
	
			// Test the model and print some stats.
			Evaluation eval = new Evaluation(data);
			eval.crossValidateModel(cls, data, 10, new Random(1));
			System.out.println(eval.toMatrixString());
			System.out.println(eval.toSummaryString());	
		} catch (Exception e) {
			System.out.println("ERROR: WEKA train the classifier error");
			e.printStackTrace();
		}
	}
	
	public String getClassification(float rssi, float lqi){
		String label="";

		// Build an Instance (representation of a "new line" of data).
		Instance inst = new DenseInstance(2); 
		inst.setValue(0, rssi);
		inst.setValue(1, lqi);
		inst.setDataset(data);

		// Classify the Instance.
		try {
			label = data.classAttribute().value((int) cls.classifyInstance(inst));
		} catch (Exception e) {
			System.out.println("ERROR: WEKA get classifier Instance error");
			e.printStackTrace();
		}

		return label;	
	}
}

