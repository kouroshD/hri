//============================================================================
// Name        : hri.cpp
// Author      : Kourosh Darvish
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <ros/ros.h>
#include <sensor_msgs/JointState.h>
#include <std_msgs/Char.h>
#include <sstream>
#include "std_msgs/String.h"
#include <stdio.h>
#include <ros/callback_queue.h>
#include "CallBackClass.hpp"
#include "cognitionClass.hpp"
#include "endorAction.hpp"
#include "endor/aonode.h"
#include "endor/aograph.h"
#include <stdlib.h>

// cout colors and options:
#define RST  "\x1B[0m"
#define KBLU  "\x1B[34m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define FBLU(x) KBLU x RST
#define FRED(x) KRED x RST
#define FGRN(x) KGRN x RST
#define BOLD(x) "\x1B[1m" x RST
using namespace std;



int main(int argc, char** argv) {
	ros::init(argc, argv, "hri");
	ros::NodeHandle nh;

	ros::Publisher pub_HMP_cmnd	=nh.advertise<std_msgs::String>("HMPCommand",1);// SA_arrow No:5
	//ros::Publisher pub_HMP_cmnd	=nh.advertise<std_msgs::String>("HMPAck",1);// SA_arrow No:5
	ros::Publisher pub_ctrl_cmnd=nh.advertise<std_msgs::String>("hri_control_command",1);//SA_arrow No:8
	//ros::Publisher pub_ctrl_cmnd=nh.advertise<std_msgs::String>("hri_control_ack",1);//SA_arrow No:8
	//CallBackClass obj_callback;// callback class of subscribers
	//cognitionClass obj_cognition;

	// Arm left=0; Arm right=1;
	const int NO_ARMS=2;
	const int NO_NODES=10;
	const int NO_NODE_ACTION_WIDTH=9;
	const int Number_of_Actions=26;

	endorActionClass obj_nodeAction(NO_NODE_ACTION_WIDTH,NO_NODES,NO_ARMS,Number_of_Actions);


	std_msgs::String msg_ctrl_cmnd[NO_ARMS] ;
	bool control_command_flag[NO_ARMS];
	int control_count[NO_ARMS], control_goal_count[NO_ARMS];

	for (int i1=0;i1<NO_ARMS;i1++)
	{
		control_command_flag[i1]=true;
		control_count[i1]=0;
		control_goal_count[i1]=0;
	}
	int rob_goal_reach_flag_counter=0;
	std::stringstream ss_ctrl_cmnd;


	// Parameter Numbers of Controller and HMP
	int noParamHMPInit=1;//Initial HMP Parameters: 		HMP type
	int noParamHMP=1+4;//Execution HMP Parameters		[0: set/get;	1-noOutputHMP:ModelsName; 4-4 Actions Parameters,]
	string modelName="HRIGHMPure"; //name of the model we use in HMP package.
	int noOutputHMP=4; // Number of output from HMP

	/* parameter and functions Initialization of the classes:*/

	//obj_cognition.NoOutputHMP=noOutputHMP;
//	obj_cognition.NO_OUTPUT_HMP=noOutputHMP;

	cognitionClass obj_cognition(noOutputHMP);
	CallBackClass obj_callback(noParamHMP,noOutputHMP);// callback class of subscribers


	obj_callback.NoOutputHMP=noOutputHMP;
	//obj_cognition.cognitionInit(noOutputHMP);

	int noParamCtrlInit=2;//Initial Control Parameters
	int noParamCtrl=6;//Execution Control Parameters, 		[1st: set/get, , 2-4: Actions Parameters,]

	float paramHMPInit [noParamHMPInit];// Initial Parameters of HMP
	float paramHMP [noParamHMP];//  Execution Parameters of HMP, first input: set Data: 1, or get Data:0
	float paramCtrlInit [noParamCtrlInit];// Initial Parameters of Controller
	float paramCtrl[noParamCtrl];// first input: set Data: 1, or get Data:0

	int max_time_rob_reach_goal=500;//sec
	int hri_ros_freq=80;//hz
	ros::Rate loop_rate(hri_ros_freq);//Based on 2nd may discussion with Fulvio.
	long int count=0;
	usleep(1e6);//microsec, make the initialization.

	bool HMP_command_flag=true;

	bool control_command_flag_1=true;

	bool Human_Gesture_Flag=true; // false from gesture recognized, until check completely gesture
	bool Gesture_Flag_Resolved=true;// false only if R should do sth, instead human invade Robot action, this is false until robot stop

	int HMP_count=0;// save count number for checking and changing flag
	char HMP_init_param_change, ctrl_init_param_change;// check yes/no for changing initial questions of hmp/ctrl
	int HMP_type_no,ctrl_type_no, ctrl_mode_no; // choosing type and mode of ctrl, hmp methods.

	cout<<"\n\n"<<BOLD(FGRN("Default HMP Type is GMM. If it is OK Press <y>,"
			" Otherwise Press <n>: "));
	cin>>HMP_init_param_change;
	// if HMP initial question:yes-> initial HMP flag to false.
	if (HMP_init_param_change=='n') {
		obj_callback.HMP_initial_command_flag=false;
		cout<<"Which HMP method do you want to use:"<<endl;
		cout<<"GMM: press 1:"<<endl<<"KF: press 2:"<<endl<<"PF: press 3 \t";
		cin>>HMP_type_no;
		paramHMPInit[0]=HMP_type_no;
	}

	cout<<BOLD(FGRN("Default Control Type is 'PID' & Default Control Mode is 'Velocity'"
			" If it is OK Press <y>, Otherwise Press <n>: "));
	cin>>ctrl_init_param_change;
	// If CTRL initial question:yes-> initial ctrl flag to false.
	if (ctrl_init_param_change=='n') {
		obj_callback.control_initial_command_flag=false;

		cout<<"Which Control Mode you want to use:"<<endl;
		cout<<"Velocity:\t press 1;"<<endl<<"Torque:\t\t press 2;"<<endl<<"Position:\t press 3: \t";
		cin>>ctrl_mode_no;

		cout<<"Which Control Type you want to use:"<<endl;
		cout<<"PID:\t\t press 1;"<<endl<<"Force: \t\t press 2;"<<endl<<"Robust:\t\t press 3;"<<endl
				<<"Nonlinear:\t press 4;"<<endl<<"Adaptive:\t press 5;"<<endl<<"MPC:\t\t press 6;\t";
		cin>>ctrl_type_no;
		paramCtrlInit[0]=ctrl_mode_no;
		paramCtrlInit[1]=ctrl_type_no;
	}

//*** 	 And-Or Graph Initialization:		***//

	// Graph Name and Location
	int ambiguity_Number=0;
	int cc=0;
	int suggested_node_number=0, suggested_action_number=0;
	string description = "/home/nasa/catkin_ws/src/hri/src/endor/assemblies/screwing_task.txt";
	string graphName="screwing_task";




	//obj_endorAct.nodeActionList();
	AOgraph myGraph = AOgraph(graphName);

	obj_nodeAction.solved_Node=myGraph.loadFromFile(description); // initialization of the first to do;
	obj_nodeAction.nodeFlag=false;

	while (ros::ok()) {

//***	Cognition		***//

	//*** Endor
		// when a node is solved, suggesting another node:
		if (count>5 && obj_nodeAction.nodeFlag==false)
		{

			//set the node as solved:
			cout<<">>>>>>>>>>>>>>>>>>>>>obj_nodeAction.solved_Node: "<<obj_nodeAction.solved_Node<<endl;

			myGraph.solveByName(obj_nodeAction.solved_Node);// (obj_nodeAction.solved_Node)
			//suggesting next node: 0/1
			if (obj_nodeAction.solved_Node=="screwedPlate_finalPos")
				exit(1);

			obj_nodeAction.suggested_Node=myGraph.suggestNext(1);//
			cout<<">>>>>>>>>>>>>>>>>>>>>obj_nodeAction.suggested_Node: "<<obj_nodeAction.suggested_Node<<endl;
			obj_nodeAction.nodeListFunction();


		}
		// when a action is solved, suggesting another action:
		//cout<<">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>actionFlag: "<<obj_nodeAction.actionFlag<<endl;

		if (Human_Gesture_Flag==false)
		{
			obj_nodeAction.humanActionSearch(obj_cognition.cognitionHMP_get(),myGraph);

		/* ambiguity_Number=0;
			// change command to action name
				//the same here!

			// check feasible nodes, --> vector of node names,
			// last node-action cannot happen because all actions related to Robot.
			//?? i think the number of for should be related to no of hyper arcs, because we check for the actions which are between the nodes
			for (int zz=0;zz<9;zz++)
			{
				// 1- check for all the feasible and not-solved nodes:
				if (myGraph.NodeFeasibiltyInfo(zz)==1 && myGraph.NodeSolveInfo(zz)==0)
				{
					cout<<"z: "<<zz<<endl;
					if (obj_nodeAction.node_action_list[zz][1]=="S")
					{
						cc=0;
						do
						{

							if (obj_nodeAction.node_action_list[zz][cc+2]==obj_cognition.cognitionHMP_get() && obj_nodeAction.node_action_flag[zz][cc]==0)
							{
								ambiguity_Number++;
								suggested_node_number=zz;
								suggested_action_number=cc+2;
								obj_nodeAction.node_action_flag[zz][cc]=-1;
								cout<<"***** "<<myGraph.NodeNameInfo(zz)<<" *****"<<endl;
								cout<<"obj_nodeAction.node_action_list[zz][cc+2]"<<obj_nodeAction.node_action_list[zz][cc+2]<<endl;
								cout<<"obj_cognition.cognitionHMP_get()"<<obj_cognition.cognitionHMP_get()<<endl;
								cout<<"obj_nodeAction.node_action_flag[zz][cc]"<<obj_nodeAction.node_action_flag[zz][cc]<<endl;
								cout<<"ambiguity_Number"<<ambiguity_Number<<endl;
								break;
							}

							cout<<"*** "<<myGraph.NodeNameInfo(zz)<<" ***"<<endl;
							cout<<"obj_nodeAction.node_action_list[zz][cc+2]"<<obj_nodeAction.node_action_list[zz][cc+2]<<endl;
							cout<<"obj_cognition.cognitionHMP_get()"<<obj_cognition.cognitionHMP_get()<<endl;
							cout<<"obj_nodeAction.node_action_flag[zz][cc]"<<obj_nodeAction.node_action_flag[zz][cc]<<endl;
							cout<<"ambiguity_Number"<<ambiguity_Number<<endl;
							cc++;
						}
						while( obj_nodeAction.node_action_list[zz][cc+2]!="0" &&( obj_nodeAction.node_action_flag[zz][cc-1]==1 || obj_nodeAction.node_action_flag[zz][cc-1]==-1) && cc <((obj_nodeAction.nodeActionList_width)-2));
						// next action not be "0", and this action flag be 1 to go forward.
						 // || obj_nodeAction.node_action_flag[zz][cc-1]==-1
					}

					else if (obj_nodeAction.node_action_list[zz][1]=="F")
					{
						cout<<myGraph.NodeNameInfo(zz)<<"\t";
						cc=0;
						do
						{	if (obj_nodeAction.node_action_list[zz][cc+2]==obj_cognition.cognitionHMP_get())
							{	ambiguity_Number++;
								break;
							}
							cc++;
						}
						while( obj_nodeAction.node_action_list[zz][cc+2]!="0" && cc <((obj_nodeAction.nodeActionList_width)-2));
					}
				}
			}
	/*		if (ambiguity_Number==1)
			{
				//&& obj_cognition.cognitionHMP_get()==obj_nodeAction.actionCommand[0]
				if (obj_nodeAction.node_number==suggested_node_number )
				{

					cout<<"ambiguity_Number: "<<ambiguity_Number<<endl;
					cout<<FBLU("Human Is Following the Path")<<endl;
					//** these should be the same!
					//obj_nodeAction.node_number=suggested_node_number;
					//obj_nodeAction.actionNumber=suggested_action_number;
					///Human_Gesture_Flag=true;
					obj_nodeAction.node_action_flag[suggested_node_number][suggested_action_number-2]=1;
					obj_nodeAction.actionFlag=false;
				}
				else if (obj_nodeAction.node_number!=suggested_node_number)
				{
					// make zero all the actions flag done on node_number now before changing the value of it.
					for (int f1=0;f1<((obj_nodeAction.nodeActionList_width)-2);f1++)
						obj_nodeAction.node_action_flag[obj_nodeAction.node_number][f1]=0;

					cout<<"ambiguity_Number: "<<ambiguity_Number<<endl;
					cout<<FRED("Human Is Not Following Optimal Path")<<endl;
					///Human_Gesture_Flag=true;
					obj_nodeAction.node_number=suggested_node_number;
					obj_nodeAction.actionNumber=suggested_action_number;
					obj_nodeAction.node_action_flag[suggested_node_number][suggested_action_number-2]=1;

					obj_nodeAction.actionFlag=false;
				}
					// send stop robot command.

				 // make the all the "-1" elements of the "node_action_flag" matrix to zero, just the ones in the "node_number" line equal to one

				for (int f1=0;f1<((obj_nodeAction.nodeActionList_width)-2);f1++)
					for (int g1=0;g1<(obj_nodeAction.Number_of_Nodes);g1++)// No of hyper arcs again, not the nodes?
						if (obj_nodeAction.node_action_flag[g1][f1]==-1 && g1!=obj_nodeAction.node_number)
							obj_nodeAction.node_action_flag[g1][f1]=0;
						else if (obj_nodeAction.node_action_flag[g1][f1]==-1 && g1==obj_nodeAction.node_number)
							obj_nodeAction.node_action_flag[g1][f1]=1;

			}
			else if (ambiguity_Number==0 && obj_cognition.cognitionHMP_get()!="ScrewingInitial")
			{
				// it means we moved at first (first putDown)
				cout<<FRED("Human Is Moved, No Path, Continue Your Actions Human")<<endl;
				///Human_Gesture_Flag=true;
				///obj_nodeAction.actionFlag=false;???

			}

			else if (ambiguity_Number>1)
			{
				cout<<"ambiguity_Number: "<<ambiguity_Number<<endl;
				cout<<FRED("******************* Ambiguity in Endor  ***********************")<<endl;
				cout<<FRED("*** Human Continue your Actions Until Ambiguity is Solved!  ***")<<endl;
				if (obj_nodeAction.responsible=="H")
				obj_nodeAction.actionFlag=false;
				/// if here, yeki  az masira behine buud, action badi elam kone,
				/// age hich kodum tu masire behine nabuud, kari nakone
			}


			*/


			if (obj_nodeAction.responsible=="H")
				Gesture_Flag_Resolved=true;
			else if (obj_nodeAction.responsible=="R")
			{
				obj_nodeAction.robStopFunction();
				for (int i1=0;i1<NO_ARMS;i1++)
				{
					msg_ctrl_cmnd[i1].data=obj_nodeAction.actionCommand[i1];
					obj_callback.rob_goal_reach_flag[i1]=true; /// ??? check to be sure
					obj_callback.hri_control_goal_flag[i1]=true;/// ??? check to be sure
					control_command_flag[i1]=false;
				}
					cout<<"control_command_flag[0]: "<<control_command_flag[0]<<endl;
					cout<<"control_command_flag[1]: "<<control_command_flag[1]<<endl;
					///obj_nodeAction.actionFlag=true;
					cout<<"POINT 10"<<endl;
					Gesture_Flag_Resolved=false;
				}
			Human_Gesture_Flag=true;
			cout<<"node_action_flag:"<<endl;
			for (int g1=0;g1<(obj_nodeAction.Number_of_Nodes);g1++)
				{for (int f1=0;f1<(obj_nodeAction.nodeActionList_width);f1++)// No of hyper arcs again, not the nodes?
					cout<<"\t"<<obj_nodeAction.node_action_flag[g1][f1];
				cout<<endl;
				}
		}


		if (obj_nodeAction.actionFlag==false && Gesture_Flag_Resolved==true)
		{
			obj_nodeAction.nodeActionListFunction();
			if (obj_nodeAction.nodeFlag==true)
			{
				obj_nodeAction.ActionListFunction();
				cout<<">> responsible: "<<obj_nodeAction.responsible<<endl;
				//cout<<">> actionCommand: "<<obj_nodeAction.actionCommand<<endl;

				if (obj_nodeAction.responsible=="H")
				{
					cout<<"****>>>>>>>>>>>>>>> Human: "<<obj_nodeAction.actionCommand[0]<<endl;
				}
				else if (obj_nodeAction.responsible=="R")
				{
					cout<<"****>>>>>>>>>>>>>>> Robot Left: "<<obj_nodeAction.actionCommand[0]<<endl;
					cout<<"****>>>>>>>>>>>>>>> Robot Right: "<<obj_nodeAction.actionCommand[1]<<endl;
					//make the control command flag false here;
					//and assign the command for the controller here

					for (int i1=0;i1<NO_ARMS;i1++)
						if (obj_nodeAction.actionCommand[i1]!="0")
						{
							msg_ctrl_cmnd[i1].data=obj_nodeAction.actionCommand[i1];
							control_command_flag[i1]=false;
							cout<<"control_command_flag "<<i1<<": "<<control_command_flag[i1]<<endl;
							cout<<"POINT 1"<<endl;
						}
						else if (obj_nodeAction.actionCommand[i1]=="0")
							obj_callback.rob_goal_reach_flag[i1]=false;

					/*if (obj_nodeAction.actionCommand[0]=="0" && obj_nodeAction.actionCommand[1]!="0")
					{
						msg_ctrl_cmnd.data=obj_nodeAction.actionCommand[1];
						control_command_flag=false;
						cout<<"control_command_flag2: "<<control_command_flag<<endl;
						obj_callback.rob_goal_reach_flag[0]=false;
						cout<<"POINT 2"<<endl;
					}*/

				}
			}
		}


	//*** HMP Cognition:
		// in this if condition we get the name of the actions coming from HMP-Detector:
		if (obj_callback.HMP_parameter_cognition_flag==false)
		{
		for(int k1=0;k1<noOutputHMP;k1++)
			obj_cognition.hmp_ActionsName[k1]=obj_callback.parameterHMP[1+k1];//??
		obj_callback.HMP_parameter_cognition_flag=true;
		}


		if (obj_callback.HMP_cognition_flag==false)
		{
			obj_cognition.cognitionHMP_set(obj_callback.HMPOutput);
			obj_cognition.cognitionHMP();
			obj_callback.HMP_cognition_flag=true;
		}

		if (obj_cognition.cognitionHMP_flag==false)
		{
			obj_cognition.reasoningHMP();

		}
	///	if (obj_cognition.reasoningHMP_flag==false && obj_nodeAction.actionFlag==true)// maybe: check responsible: "H"
			if (obj_cognition.reasoningHMP_flag==false )// maybe: check responsible: "H"
		{
			cout<<">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"<<endl;
			cout<<" Recognized Action:\t"<<obj_cognition.cognitionHMP_get()<<endl;
			cout<<">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"<<endl;
			// if the action recognized by hmp reasoning == action suggested
				//previously by andorAction, it means it is solved and another action should be suggested
			// we should delete next if condition:
/*			if (obj_cognition.cognitionHMP_get()==obj_nodeAction.actionCommand[0])// human action just 0 now...
			{
				obj_nodeAction.actionFlag=false;
			}
	*/
			Human_Gesture_Flag=false;
		}


//***	HMP		***//

	// HMP Initial Parameters
		if (obj_callback.HMP_initial_command_flag==false) {

			obj_callback.NoParameterHMP=noParamHMPInit;
			std_msgs::String msg_HMPPar;
			std::stringstream ss_HMPPar;
			for (int ii=1; ii<= noParamHMPInit; ii++) {
				ss_HMPPar<<paramHMPInit[ii-1];
			}
			msg_HMPPar.data=ss_HMPPar.str();
			ROS_INFO("I publish HMP Initial command: %s",msg_HMPPar.data.c_str());
			pub_HMP_cmnd.publish(msg_HMPPar);
		}

	//	Higher level HMP

		if (count <=1 && obj_callback.HMP_initial_command_flag==true) {
			HMP_command_flag=false;
			obj_callback.HMP_ack_flag=false;
			HMP_count=count;

			paramHMP[0]=0.0;
			for (int ii=2; ii<= noParamHMP; ii++) {
				 paramHMP[ii-1]=1.0/(1+ii);
			 }
			obj_callback.NoParameterHMP=noParamHMP;
		}

	// Flag check
		if (count>=HMP_count+5 &&obj_callback.HMP_ack_flag==false) {
			HMP_command_flag=false;
			HMP_count=count;
		}

	 //	Publish HMP Parameter Message
		if ( HMP_command_flag==false ) {
			std_msgs::String msg_HMPPar;
			std::stringstream ss_HMPPar;
			for (int ii=1; ii<= noParamHMP; ii++) {
				ss_HMPPar<<paramHMP[ii-1]<<" ";
			}
			cout<<endl;
			msg_HMPPar.data=ss_HMPPar.str();
			ROS_INFO("I publish HMP command: %s",msg_HMPPar.data.c_str());
			pub_HMP_cmnd.publish(msg_HMPPar);
			HMP_command_flag=true;
		}

//***	Control		***//

	// Control Initial Parameters
		if (obj_callback.control_initial_command_flag==false) {
			std_msgs::String msg_ctrlPar;
			std::stringstream ss_ctrlPar;
			for (int ii=1; ii<= noParamCtrlInit; ii++) {
				ss_ctrlPar<<paramCtrlInit[ii-1];
			}
			msg_ctrlPar.data=ss_ctrlPar.str();
			ROS_INFO("I publish Control Initial command: %s",msg_ctrlPar.data.c_str());
			pub_ctrl_cmnd.publish(msg_ctrlPar);
		}

	//	Higher Level Control
		// in this if condition when the robot reaches the goal we check for the next action to do.

		//if ( obj_callback.control_ack_flag[i1]==true &&	obj_nodeAction.actionCommand[i1]!="0" && obj_nodeAction.actionCommand[1]!="0" && obj_callback.hri_control_goal_flag[0]==false && obj_callback.hri_control_goal_flag[1]==true )
	/*	for (int i1=0;i1<NO_ARMS;i1++)
			if ( obj_callback.control_ack_flag[i1]==true &&	obj_nodeAction.actionCommand[i1]!="0" && obj_callback.hri_control_goal_flag[i1]==false  )
			{
				msg_ctrl_cmnd[i1].data=obj_nodeAction.actionCommand[i1];
				control_command_flag[i1]=false;
				cout<<"control_command_flag "<<i1<<": "<<control_command_flag[i1]<<endl;
				cout<<"POINT 3"<<endl;
			}*/

		///&& Human_Gesture_Flag==true
		rob_goal_reach_flag_counter=0;
		for (int i1=0;i1<NO_ARMS;i1++)
			if (obj_callback.rob_goal_reach_flag[i1]==false )
				rob_goal_reach_flag_counter++;
		if(rob_goal_reach_flag_counter==NO_ARMS && Gesture_Flag_Resolved==true)
		{
			obj_nodeAction.actionFlag=false;
			for (int i1=0;i1<NO_ARMS;i1++)
				obj_callback.rob_goal_reach_flag[i1]=true; /// ??? check to be sure
			obj_nodeAction.node_action_flag[obj_nodeAction.node_number][obj_nodeAction.actionNumber]=1;

			for (int g1=0;g1<(obj_nodeAction.Number_of_Nodes);g1++)
				{for (int f1=0;f1<(obj_nodeAction.nodeActionList_width);f1++)// No of hyper arcs again, not the nodes?
					cout<<"\t"<<obj_nodeAction.node_action_flag[g1][f1];
				cout<<endl;
				}
		}
		else if(rob_goal_reach_flag_counter==NO_ARMS && Gesture_Flag_Resolved==false)
		{
			for (int i1=0;i1<NO_ARMS;i1++)
				obj_callback.rob_goal_reach_flag[i1]=true; /// ??? check to be sure
			Gesture_Flag_Resolved=true;
		}

/*		if (obj_callback.rob_goal_reach_flag[0]==false && obj_callback.rob_goal_reach_flag[1]==false && Human_Gesture_Flag==false){

				obj_callback.rob_goal_reach_flag[0]=true; /// ??? check to be sure
				obj_callback.rob_goal_reach_flag[1]=true; /// ??? check to be sure
				Human_Gesture_Flag=true;
				obj_nodeAction.actionFlag=false;
			}
*/


		// for initial control checking
		if (count<= 1&& obj_callback.control_initial_command_flag==true)
		{
			control_command_flag[0]=false;
			cout<<"control_command_flag[0]: "<<control_command_flag[0]<<endl;
///	16 june		obj_callback.control_ack_flag=false;//??? change control->HMP, see what is happening??
			///control_count=count;
			for (int ii=1; ii<= noParamCtrl; ii++) {
				 paramCtrl[ii-1]=0;
				 ss_ctrl_cmnd<<paramCtrl[ii-1];
			 }
			msg_ctrl_cmnd[0].data=ss_ctrl_cmnd.str();
		}

	// Flag check
		// flag for checking if the controller reach the goal, wait for 10 sec(10sec*80 hz)
		for (int i1=0;i1<NO_ARMS;i1++)
		{
			if (count>=control_goal_count[i1]+(max_time_rob_reach_goal*hri_ros_freq)
					&& obj_callback.hri_control_goal_flag[i1]==false && obj_nodeAction.actionCommand[i1]!="0")
			{
				control_command_flag[i1]=false;//*** make this flag false in real test
				cout<<"control_command_flag["<<i1<<"]: "<<control_command_flag[i1]<<endl;
				msg_ctrl_cmnd[i1].data=obj_nodeAction.actionCommand[i1];
				obj_callback.hri_control_goal_flag[i1]=true;// not sure check later
			}
				// check for receiving cmnd from hri -> controller:
				if (count>=control_count[i1]+10 &&obj_callback.control_ack_flag[i1]==false)
					control_command_flag[i1]=false;//*** make this flag false in real test
				///	cout<<"control_command_flag9: "<<control_command_flag<<endl;
				/*		if (count>=control_goal_count+(max_time_rob_reach_goal*hri_ros_freq)
											&& obj_callback.hri_control_goal_flag[1]==false && obj_nodeAction.actionCommand[1]!="0")
							{
								control_command_flag=true;//*** make this flag false in real test
								cout<<"control_command_flag8: "<<control_command_flag<<endl;
								msg_ctrl_cmnd.data=obj_nodeAction.actionCommand[1];
								obj_callback.hri_control_goal_flag[1]=true;// not sure check later
							}*/



		}



	// Publish Control Message
		if (count<=1)
		{

		}
			for (int i1=0;i1<NO_ARMS;i1++)
				if ( control_command_flag[i1]==false )
				{

					ROS_INFO("I publish Control Command: %s",msg_ctrl_cmnd[i1].data.c_str());
					pub_ctrl_cmnd.publish(msg_ctrl_cmnd[i1]);
					control_command_flag[i1]=true;
					obj_callback.control_ack_flag[i1]=false;
					control_count[i1]=count;
					control_goal_count[i1]=count;

				}


		if (count==0){	usleep(0.5e6); }
		loop_rate.sleep();
		ros::spinOnce();
		count++;
	}
	return 0;
}