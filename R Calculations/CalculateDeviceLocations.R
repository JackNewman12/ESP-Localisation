# Code to calculate the location of WiFi devices
# Sorry this code is a mess. It was basically a scratch pad that I used for finding reasonable results.
# You basically need to comment / uncomment code to get it to work in different modes.
# To be honest you probably should just rewrite this in python or something

require(plotrix)

#Grab Text File
mydata = read.table("TestingData.txt",stringsAsFactors=FALSE) 
myDataFrame <- data.frame(mydata) 

# Manuall add some devices we know. And their fine locations
y <- data.frame(A=c("18:FE:34:CE:8F:63",3.85,0.93),B=c("5C:CF:7F:18:EA:62",0.15,3),C=c("18:FE:34:CE:D8:3E",1.93,1.65),D=c("18:FE:34:CE:DA:AF",0.95,1.35),E=c("18:FE:34:CE:DA:8D",3.07,3.17),stringsAsFactors = FALSE)



###############################################
CalculateLocations <- function(x){
	#print(x)
	totalAverageError <- 0
	sumOfErrors <- 0
	memes <- 0
	for (swag in 0:4){
	for (col in 1:ncol(y)){
		individualData <- subset(myDataFrame, myDataFrame[,3] == y[,col][1]) 
		
		#individualData[[4]] <- 2.5 * 10^(-50/x[1] - individualData[[4]]/x[1])  #Convert the RSSI to distance
		individualData[[4]] <- x[2] * 10^(x[3]/x[1] - individualData[[4]]/x[1])  #Convert the RSSI to distance


		dataPoints <- individualData[!duplicated(individualData[,c(1,2)]),c(1,2)] #All the Unique data points (As in all the sensors)
		#print(expand.grid(dataPoints[,1])
	
		
		dataPoints[3] <- 0 #Add another column for the averages

		for (z in 1:length(dataPoints[,1])) {  #For each of the sensors, average what they we`re reading
			Averager = subset(individualData, individualData[,1] == dataPoints[,1][z] & individualData[,2] == dataPoints[,2][z])
			dataPoints[,3][z] <- colMeans(Averager[4])
	
		}

		dataPoints$V3[dataPoints$V3 < x[5]] <- x[5] + x[6]
		dataPoints[,3] = sqrt(dataPoints[,3]^2 - x[7]^2)
		


		data_ref <- data.frame(x=c(dataPoints[,1]),y=c(dataPoints[,2]),r = c(dataPoints[,3]))  #The averaged data that the maths is going to be performed on
		norm_vec <- function(y) sum(abs((sqrt((y[1]-data_ref$x)^2+(y[2]-data_ref$y)^2))-data_ref$r)^x[4])
		Firstresult = nlm(norm_vec,c(0,0),steptol=0.1)   #Two different maths, both normally end up giving basically the same answer


		n = nrow(dataPoints) #How many sensors are there
		#l = t(combn(n,n-1))	#all combinations of n-1 datapoints
		l  = t(combn(n,n-swag))
		#l = t(combn(n,n-x[8]))	#all combinations of n-1 datapoints

		LargestError <- 0
		LargestErrorIndex <- 1

		SmallestRealError <- 99
		SmallestRealErrorIndex <- 1
		averageError <- 0
		for (combination in 1:nrow(l)){
			dataPoints2 <- dataPoints[l[combination,],]
			#print(dataPoints2)
		
		
			data_ref <- data.frame(x=c(dataPoints2[,1]),y=c(dataPoints2[,2]),r = c(dataPoints2[,3]))  #The averaged data that the maths is going to be performed on
			norm_vec <- function(y) sum(abs((sqrt((y[1]-data_ref$x)^2+(y[2]-data_ref$y)^2))-data_ref$r)^x[4])


			result = nlm(norm_vec,c(0,0),steptol=0.1)   #Two different maths, both normally end up giving basically the same answer
			error <- sqrt((result[[2]][1] - Firstresult[[2]][1])^2 + (result[[2]][2] - Firstresult[[2]][2])^2)
			realError <- sqrt(        (result[[2]][1] - as.numeric(y[,col][2]))^2      +        (result[[2]][2] - as.numeric(y[,col][3]))^2)  	
			averageError <- averageError + realError
			if(error > LargestError ){
				LargestError <- error
				LargestErrorIndex <- combination
			}
			if(realError < SmallestRealError){
			SmallestRealError <- realError
			SmallestRealErrorIndex <- combination
			}				
			
			#print(error)
			#print(realError)
		}
		averageError <- averageError / nrow(l)
		totalAverageError <- totalAverageError + averageError
		#print(averageError)
		#print(LargestError)
		#print(LargestErrorIndex)
		#print(SmallestRealErrorIndex)

		#dataPoints3 <- dataPoints[l[LargestErrorIndex,],] #Get what we think is probably the best answer
		#dataPoints3 <- dataPoints[l[SmallestRealErrorIndex,],] #CHEAT MODE

		#data_ref <- data.frame(x=c(dataPoints3[,1]),y=c(dataPoints3[,2]),r = c(dataPoints3[,3]))  #The averaged data that the maths is going to be performed on
		#norm_vec <- function(y) sum(abs((sqrt((y[1]-data_ref$x)^2+(y[2]-data_ref$y)^2))-data_ref$r)^x[4])


		#result = nlm(norm_vec,c(0,0),steptol=0.1)   #Two different maths, both normally end up giving basically the same answer


		#print(y[,col][1])
		#print(result[[2]])
		#print("------------")
		#sumOfErrors <- sumOfErrors + sqrt(        (result[[2]][1] - as.numeric(y[,col][2]))^2      +        (result[[2]][2] - as.numeric(y[,col][3]))^2)
		
		#draw.circle(dataPoints[,1],dataPoints[,2],dataPoints[,3],border=col) 		#Draw radius circles from each sensor
		#draw.circle(result[[2]][1],result[[2]][2],0.05,border="black",col="black") 	#Draw those answers
		
		#draw.circle(as.numeric(y[,col][2]),as.numeric(y[,col][3]),0.05,border="blue",col="blue")  #Draw correct answers
		#segments(as.numeric(y[,col][2]),as.numeric(y[,col][3]),result[[2]][1],result[[2]][2])	#Draw line between correct answer and calculated answer
		
	}

	memes <- totalAverageError / ncol(y)
	print(memes)
	}
	return(memes / 4)

}
#############################################



plot(-999,-999,xlim=c(-1,7),ylim=c(-1,7))   #Empty Plot
test <- nlminb(c(50,2.5,-50,4,2.6,0.1,2.5),CalculateLocations,control=list(abs.tol=0.1,rel.tol=0.1,step.min=0.1))
print(test)
#plot(-999,-999,xlim=c(-1,7),ylim=c(-1,7))   #Empty Plot
CalculateLocations(test[[1]])

