#!/usr/bin/env ruby

#1st argumnt to script should be full path to filename
inputFilePath = ARGV[0]

#1st argumnt to script should be full path to filename
outputFilePath = ARGV[1]

#2nd argument should be cachesize
cache1 = ARGV[2]

outFile = File.open(outputFilePath,'w')

level1 = cache1.to_i

#Array for storing function objects
funcArr = []


class FuncRanking
    
    attr_accessor :functionName
    attr_accessor :missCounts1
    attr_accessor :missCounts2
    attr_accessor :diffCount1
    attr_accessor :diffCount2
    
    
    def initialize (funcName,missCount1,missCount2,diffMissCount1,diffMissCount2)
        
        @functionName = funcName
        @missCounts1 = missCount1
        @missCounts2 = missCount2
        @diffCount1 = diffMissCount1
        @diffCount2 = diffMissCount2
        
    end
    
    def display
    
        puts @functionName + " :: " + @diffCount.to_s()
        
    end
    
end


missCount1 = 0
missCount2 = 0
funcName = 0;

#strr = " Cache Size 1 is: " + cache1.to_s() + "  Cache Size 2 is: " cache2.to_s() + " \n \n"
#outFile.write(strr);

totalMC1 = 0;
totalMC2 = 0;


#Count of functions
funcCount = 0


File.open(inputFilePath,'r') do |lines|
   
    if lines.respond_to?("each")
    
        lines.each do |line|
            
            strLine = line.to_s()
            
            if strLine.include? "Called Function Address"
            
                funcName = strLine.split(':');
                
                # Miss Count for two granularities for each function
                missCount1 = 0;
                missCount2 = 0;
                
                
            end
            
            # 5th index for reuse distance cache size 1
            # 9th index for reuse time count cache size 1
            # 15th index for reuse distance cache size 1
            # 19th index for reuse time count cache size 1
            
            if strLine.include? "CSize"
                
                funcContent = strLine.split(' ');
                
                if funcContent[5].to_i() >= level1
                    
                    missCount1 += funcContent[9].to_i
                
                end
                
                if funcContent[15].to_i() >= level1
                    
                    missCount2 += funcContent[19].to_i
                end
                
            end
            
            
            if strLine.include? "END OF FUNCTION"
                
                diffOfMissCounts1 = missCount1 - missCount2
                diffOfMissCounts2 = missCount1 - missCount2/16
                totalMC1 += missCount1
                
                totalMC2 += (missCount2/16)
                
                #  outString = " "
                funName = funcName[1].to_s().strip()
                
                funcObject = FuncRanking.new(funName,missCount1,missCount2,diffOfMissCounts1,diffOfMissCounts2)
                
                funcArr.insert(-1,funcObject)
                
            end
            
        end

    end
    
end


funcArr.sort!{ |a,b| b.diffCount2 <=> a.diffCount2}


funcArr.each do |func|
    # puts func.display
    
    funcCount +=1
    outString = funcCount.to_s + " " +func.functionName + " Miss Count Cache 1: "+ func.missCounts1.to_s + " Miss Count Cache 2: " +  func.missCounts2.to_s + " Difference 1 of MissCounts is: " + func.diffCount1.to_s + " Difference 2 of MissCounts is : " + func.diffCount2.to_s + "\n"
    
    outFile.write(outString);
end

diffMiss = totalMC1 - totalMC2
puts "Mis Count 1 = " + totalMC1.to_s() + "  Miss Count 2 = " + totalMC2.to_s() + " \n "
puts "Difference of Miss Counts is: " + diffMiss.to_s()