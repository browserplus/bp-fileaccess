#!/usr/bin/env ruby
 require 'logger'
 
  $success = 0
  $fail = 0
  $testid = 0
  
#  def assert_log(arg1, arg2, testid, log, success, fail)
#	testid = testid + 1
#	if arg1 != arg2
#		#log = Logger.new('log.txt')
#		log.error "#{testid} FAILED. #{arg1} was expected. #{arg2} is what was returned  "
#		fail = fail+1
#	end
#	if arg1==arg2
#		log.info "testId: #{testid} <-- SUCCESS. "
#		puts "SUCK"
#		success = success + 1
#	end
#  end	
 
 
 def assert_log(arg1, arg2, log, descrip, testid)
	$testid = $testid + 1
	if arg1 != arg2
		log = Logger.new('log.txt')
		log.error "File: #{descrip}  testId: #{testid} FAILED. #{arg1} was expected. #{arg2} is what was returned  "
		$fail = $fail+1
	end
	if arg1==arg2
		log.info "File: #{descrip}  testId: #{testid} <-- SUCCESS. "
		$success = $success + 1
	end
   
 end
 
 
 def assert_runTime(got, result, log, descrip, testid)
	$testid = $testid + 1
	if result==false
		log.error "File: #{descrip}  testId: #{testid} FAILED. RunTime Error not raised. Instead, #{got} was raised "
		$fail = $fail+1
	end
	if result==true
		log.info "File: #{descrip}  testId: #{testid} <-- SUCCESS. "
		$success = $success + 1
	end
 
 end
 
 def printresult
	puts "TOTAL NUMBER OF TESTS: #{$testid} "
	puts "Success: #{$success} "
	puts "Failures: #{$fail} "
 end
 
